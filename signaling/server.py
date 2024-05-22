import asyncio, json, uuid, time, logging, sys, base64, binascii
from enum import IntEnum
from copy import copy

logger = logging.getLogger(__name__)

# GLOBALS
SERVER_ID = b'\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff'
CONNECTIONS = {}
DELIMINATER = b"-+"

class Signal_message_type(IntEnum):
	SIGNAL_START_ADVERTISING = 1
	SIGNAL_STOP_ADVERTISING = 2
	SIGNAL_REQUEST_ADVERTISERS = 3
	SIGNAL_SOLICIT_ADS = 4
	SIGNAL_GAME_AD = 5
	SIGNAL_JUICE_LOCAL_DESCRIPTION = 101
	SIGNAL_JUICE_CANDIDATE = 102
	SIGNAL_JUICE_DONE = 103
	SERVER_SET_ID = 254
	SERVER_ECHO = 255    


class Signal_packet():
    _peer_ID = b''
    message_type = 0
    data = ""
    
    def __init__(self,json_string = False):
        if json_string:
            self.as_json = json_string
            
    @property
    def peer_ID(self):
        return self._peer_ID

    @peer_ID.setter
    def peer_ID(self, value:bytes):
        if len(value) == 16:
            self._peer_ID = value;
        else:
            raise ValueError(f"bytes object should be exactly 16 bytes for ID, received {len(value)}")
        
    @property
    def peer_ID_base64(self):
        return base64.b64encode(self._peer_ID);

    @peer_ID_base64.setter
    def peer_ID_base64(self, value):
        try:
            bytesvalue = base64.b64decode(value)
            if len(bytesvalue) == 16:
                self._peer_ID = bytesvalue
            else:
                raise ValueError("computed bytes object should be exactly 16 bytes for ID")
        except binascii.Error as e:
            logger.error(f"cannot set packet peer_ID_base64 to {value}, error: {e}")
        
    @property
    def as_json(self):
        return json.dumps(
            {
                "peer_ID":self.peer_ID_base64.decode(),
                "message_type":self.message_type,
                "data":self.data
            });
    
    @as_json.setter
    def as_json(self,json_string):
        if isinstance(json_string,bytes):
            json_string = json_string.decode()
        try:
            j = json.loads(json_string)
            self.peer_ID_base64 = j["peer_ID"]
            self.message_type = j["message_type"]
            self.data = j["data"]
        except Exception as e:
            print(f"JSON ERROR: {e}, when processing string {json_string}")

class ServerProtocol(asyncio.Protocol):
    _peer_ID = b''
    remainder = None

    def __init__(self):
        self.peer_ID = uuid.uuid4().bytes

    @property
    def peer_ID(self):
        return self._peer_ID

    @peer_ID.setter
    def peer_ID(self, value:bytes):
        if len(value) == 16:
            self._peer_ID = value;
        else:
            raise ValueError("bytes object should be exactly 16 bytes for ID")
        
    @property
    def peer_ID_base64(self):
        return base64.b64encode(self._peer_ID);

    @peer_ID_base64.setter
    def peer_ID_base64(self, value):
        try:
            bytesvalue = base64.b64decode(value)
            if len(bytesvalue) == 16:
                self._peer_ID = bytesvalue
            else:
                raise ValueError("computed bytes object should be exactly 16 bytes for ID")
        except binascii.Error as e:
            logger.error(f"cannot set peer_ID_base64 to {value}, error: {e}")
        
    def connection_made(self, transport):
        self.transport = transport
        self.advertising = False
        self.addr = transport.get_extra_info('peername')
        global CONNECTIONS
        CONNECTIONS[self.peer_ID] = self
        logger.info(f"new conn: {self.addr} assigned: {self.peer_ID_base64}")
        
        send_buffer = Signal_packet()
        send_buffer.peer_ID = SERVER_ID
        send_buffer.message_type = Signal_message_type.SERVER_SET_ID
        send_buffer.data = self.peer_ID_base64
        self.send_packet(send_buffer)

    def connection_lost(self,exc):
        logger.info(f"connection lost to {self.peer_ID_base64}")
        del CONNECTIONS[self.peer_ID]
    
    def error_received(self,exc):
        logger.error(f"error: {exc}")
    
    def send_packet(self, packet: Signal_packet):
        logger.debug(f"from: {packet.peer_ID_base64}, to: {self.peer_ID_base64}, type: {packet.message_type} data: {packet.data}")
        logger.debug(packet.as_json)
        self.transport.write(packet.as_json.encode()+DELIMINATER)

    def data_received(self,data):
        logger.debug(f"Data received from {self.addr}: {data}")
        
        if (self.remainder):
            data = self.remainder+data
            self.remainder = None
        raw_packets = data.split(DELIMINATER)
        if raw_packets[-1][-len(DELIMINATER):] != DELIMINATER:
            self.remainder = raw_packets.pop()


        for raw_packet in raw_packets:
            if not len(raw_packet):
                continue
            packet = Signal_packet(raw_packet)
            match(Signal_message_type(packet.message_type)):
                case Signal_message_type.SERVER_SET_ID:
                    old_peer_ID = self.peer_ID_base64
                    logger.info(f"peer {old_peer_ID} requested ID change to {packet.data}")
                    self.peer_ID_base64 = packet.data
                    
                case Signal_message_type.SIGNAL_START_ADVERTISING:
                    self.advertising = True;
                    logger.info(f"{self.peer_ID_base64} started advertising")
                    
                case Signal_message_type.SIGNAL_STOP_ADVERTISING:
                    self.advertising = False;
                    logger.info(f"{self.peer_ID_base64} stopped advertising")
                    
                case Signal_message_type.SIGNAL_REQUEST_ADVERTISERS:
                    advertisers = list({i for i in CONNECTIONS if CONNECTIONS[i].advertising})
                    if self.peer_ID in advertisers: advertisers.remove(self.peer_ID)
                    ads_b64 = []
                    for peer in advertisers:
                        ads_b64.append(CONNECTIONS[peer].peer_ID_base64.decode())

                    if len(ads_b64):
                        send_buffer = Signal_packet()
                        send_buffer.peer_ID = SERVER_ID
                        send_buffer.message_type = Signal_message_type.SIGNAL_REQUEST_ADVERTISERS
                        send_buffer.data = "".join(ads_b64)
                        logger.debug(f"Send buffer: {send_buffer.data}")
                        self.send_packet(send_buffer)
                case _:
                    # all other signals go peer for now
                    if packet.peer_ID in CONNECTIONS.keys():
                        send_buffer = copy(packet)
                        # replace from & to peer IDs
                        send_buffer.peer_ID = self.peer_ID
                        CONNECTIONS[packet.peer_ID].send_packet(send_buffer)
                    else:
                        logger.error(f"{packet.peer_ID_base64} not connected")

async def main():
    logging.basicConfig(filename='signalingserver.log', level=logging.DEBUG)
    logger.info("Starting TCP Server")
    print("Starting TCP server")

    loop = asyncio.get_running_loop()

    server = await loop.create_server(
        lambda: ServerProtocol(),
        None,9988
    )
    
    async with server:
        await server.serve_forever()



if __name__ == "__main__":
    asyncio.run(main())