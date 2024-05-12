
#include "DirectIP.h"

#include "Output.h"
#include "UDPSocket.h"
#include "SettingsDialog.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static void sleep(unsigned int secs) { Sleep(secs * 1000); }
#define BUFFER_SIZE 4096
#include <string>



//static void copy_clipboard(const char* output) {
//    const size_t len = strlen(output) + 1;
//    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
//    memcpy(GlobalLock(hMem), output, len);
//    GlobalUnlock(hMem);
//    OpenClipboard(0);
//    EmptyClipboard();
//    SetClipboardData(CF_TEXT, hMem);
//    CloseClipboard();
//}

namespace DRIP
{
  char nName[] = "Direct IP";
  char nDesc[] = "";


  SNP::NetworkInfo networkInfo = { nName, 'DRIP', nDesc,
    // CAPS:
  {sizeof(CAPS), 0x20000003, SNP::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}};

  UDPSocket session;
  SignalingSocket signaling_socket;
  

  // ----------------- game list section -----------------------
  Util::MemoryFrame adData;
  bool isAdvertising = false;
  //SIGNALING need

  // --------------   incoming section  ----------------------
  char recvBufferBytes[1024];

  // ---------------  packet IDs  ------------------------------
  const int PacketType_RequestGameStats = 1;
  const int PacketType_GameStats = 2;
  const int PacketType_GamePacket = 3;


  //------------------------------------------------------------------------------------------------------------------------------------
  void rebind()
  {
    int targetPort = atoi(getLocalPortString());
    if(session.getBoundPort() == targetPort)
      return;
    try
    {
      session.release();
      session.init();
      session.setBlockingMode(false);
      session.bind(targetPort);
      setStatusString("network ready");
    }
    catch(...)
    {
      setStatusString("local port fail");
    }
  }
  void DirectIP::processIncomingPackets()
  {

    try
    {
      // receive all packets
      while(true)
      {
        // receive next packet
        UDPAddr sender;
        Util::MemoryFrame packet = session.receivePacket(sender, Util::MemoryFrame(recvBufferBytes, sizeof(recvBufferBytes)));
        if(packet.isEmpty())
        {
          if(session.getState() == WSAECONNRESET)
          {
//            DropMessage(1, "target host not reachable");
            setStatusString("host IP not reachable");
            continue;
          }
          if(session.getState() == WSAEWOULDBLOCK)
            break;
          throw GeneralException("unhandled UDP state");
        }

        memset(sender.sin_zero, 0, sizeof(sender.sin_zero));

        int type = packet.readAs<int>();
        if(type == PacketType_RequestGameStats)
        {
          // -------------- PACKET: REQUEST GAME STATES -----------------------
          if(isAdvertising)
          {
            // send back game stats
            char sendBufferBytes[600];
            Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
            Util::MemoryFrame spacket = sendBuffer;
            spacket.writeAs<int>(PacketType_GameStats);
            spacket.write(adData);
            session.sendPacket(sender, sendBuffer.getFrameUpto(spacket));
          }
        }
        else
        if(type == PacketType_GameStats)
        {
          // -------------- PACKET: GAME STATS -------------------------------
          // give the ad to storm
          passAdvertisement(sender, packet);
        }
        else
        if(type == PacketType_GamePacket)
        {
          // -------------- PACKET: GAME PACKET ------------------------------
          // pass strom packet to strom
          passPacket(sender, packet);
        }
      }
    }
    catch(GeneralException &e)
    {
      DropLastError("processIncomingPackets failed: %s", e.getMessage());
    }
  }
  //------------------------------------------------------------------------------------------------------------------------------------
  //------------------------------------------------------------------------------------------------------------------------------------
  void DirectIP::initialize()
  {
    showSettingsDialog();


    // bind to port
    rebind();

  }
  void DirectIP::destroy()
  {
    hideSettingsDialog();
    session.release();
    // destroy juice agents & signalling
  }
  void DirectIP::requestAds()
  {
    rebind();
    processIncomingPackets(); //receive()

    // send game state request
    char sendBufferBytes[600];
    Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
    Util::MemoryFrame ping_server = sendBuffer;
    ping_server.writeAs<int>(PacketType_RequestGameStats);
    // sending a single int to request game state
    // replace with signaling?

    UDPAddr host;
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = inet_addr(getHostIPString());
    host.sin_port = htons(atoi(getHostPortString()));
    session.sendPacket(host, sendBuffer.getFrameUpto(ping_server));
    //JUICE
    std::string msg;
    msg += Signal_message_type(SERVER_REQUEST_ADVERTISERS);
    signaling_socket.send_packet(signaling_socket.server, msg);
  }
  void DirectIP::sendAsyn(const UDPAddr& him, Util::MemoryFrame packet)
  {

    processIncomingPackets(); //receive()

    // create header
    char sendBufferBytes[600];
    Util::MemoryFrame sendBuffer(sendBufferBytes, 600);
    Util::MemoryFrame spacket = sendBuffer;
    spacket.writeAs<int>(PacketType_GamePacket);
    spacket.write(packet);

    // send packet
    session.sendPacket(him, sendBuffer.getFrameUpto(spacket));
  }
  void DirectIP::receive()
  {
    // receive signaling
    // receive from juice agents (queue)
    processIncomingPackets();
  }
  void DirectIP::startAdvertising(Util::MemoryFrame ad)
  {
    rebind();
    adData = ad;
    isAdvertising = true;
    //JUICE
    std::string msg;
    msg += Signal_message_type(SERVER_START_ADVERTISING);
    signaling_socket.send_packet(signaling_socket.server, msg);
  }
  void DirectIP::stopAdvertising()
  {
    isAdvertising = false;
    //JUICE
    std::string msg;
    msg += Signal_message_type(SERVER_STOP_ADVERTISING);
    signaling_socket.send_packet(signaling_socket.server, msg);
  }
  //------------------------------------------------------------------------------------------------------------------------------------
};
