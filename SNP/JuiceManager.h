#pragma once

#include <juice.h>
#include <string>
#include <unordered_map>
#include "signaling.h"
#include "Util/MemoryFrame.h"
#include "iostream"
#include "concurrentqueue.h"
#include "SNETADDR.h"

enum Juice_signal {
	juice_signal_local_description = 1,
	juice_signal_candidate = 2,
	juice_signal_gathering_done = 3
};

class JuiceWrapper
{
public:
	JuiceWrapper(const SNETADDR& ID, signaling::SignalingSocket& sig_sock,
		moodycamel::ConcurrentQueue<std::string>* receive_queue,
		std::string init_message);
	~JuiceWrapper();
	void signal_handler(const signaling::Signal_packet packet);
	void send_message(const std::string& msg);
	void send_message(const char* begin, const size_t size);
	void send_message(Util::MemoryFrame frame);
	juice_state p2p_state;
	moodycamel::ConcurrentQueue<std::string>* p_receive_queue;
	SNETADDR m_ID;

private:
	void send_signaling_message(char* msg, Juice_signal msgtype);
	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);


	juice_config_t m_config;
	const std::string m_stun_server = "stun.l.google.com";
	const int m_stun_server_port = 19302;
	signaling::SignalingSocket m_signaling_socket;
	juice_agent_t *m_agent;
	char m_sdp[JUICE_MAX_SDP_STRING_LEN];

};


class JuiceMAN
{
public:
	JuiceMAN(signaling::SignalingSocket& sig_sock,
		moodycamel::ConcurrentQueue<std::string>* receive_queue)
		: m_agents(), m_signaling_socket(sig_sock), p_receive_queue(receive_queue)
	{};
	~JuiceMAN() {};

	void create_if_not_exist(const std::string& ID);
	void send_p2p(const std::string& ID, const std::string& msg);
	void send_p2p(const std::string& ID, Util::MemoryFrame frame);
	//void signal_handler(const std::string& source_ID, const std::string& msg);
	void signal_handler(const signaling::Signal_packet packet);
	void send_all(const std::string&);
	void send_all(const char* begin, const size_t size);
	void send_all(Util::MemoryFrame frame);
	juice_state peer_status(SNETADDR peer_ID);

private:
	std::unordered_map<std::string,JuiceWrapper*> m_agents;
	signaling::SignalingSocket m_signaling_socket;
	moodycamel::ConcurrentQueue<std::string>* p_receive_queue;
};

