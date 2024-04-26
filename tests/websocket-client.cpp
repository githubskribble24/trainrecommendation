#include <network-monitor/websocket-client.h>

#include <openssl/ssl.h>
#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/beast/ssl.hpp>

#include <iostream>
#include <string>
#include <filesystem>

using NetworkMonitor::WebSocketClient;

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_CASE(cacert_pem)
{
	// TEST_CACERT_PEM definition is set in CMake configuration
	BOOST_CHECK(std::filesystem::exists(TESTS_CACERT_PEM));
}

BOOST_AUTO_TEST_CASE(class_WebSocketClient)
{
	// Connection targets
	const std::string url 			{"ltnm.learncppthroughprojects.com"};
	const std::string endpoint 	{"/echo"};
	const std::string port 			{"443"};
	const std::string message 	{"Hello WebSocket"};

	// TLS context
	boost::asio::ssl::context ctx { boost::asio::ssl::context::tlsv12_client };
	ctx.load_verify_file(TESTS_CACERT_PEM);

	// Always start with an I/O context object.
	boost::asio::io_context ioc {};

	// The class under test
	WebSocketClient client { url, endpoint, port, ioc, ctx };

	// We use these flags to check the connection, send, receive functions
	//	work as expected
	bool connected 				{false};
	bool messageSent 			{false};
	bool messageReceived 	{false};
	bool messageMatches 	{false};
	bool disconnected 		{false};

	// Our own callbacks
	auto onSend { [&messageSent](auto ec) {
		messageSent = !ec;
	}};

	auto onConnect { [&client, &connected, &onSend, &message](auto ec) {
		connected = !ec;
		if (!ec)
		{
			client.Send(message, onSend);
		}
	}};

	auto onClose { [&disconnected](auto ec) {
		disconnected = !ec;
	}};

	auto onReceive { [&client,
										&onClose,
										&messageReceived,
										&messageMatches,
										&message](auto ec, auto received) {
		messageReceived = !ec;
		messageMatches = message == received;
		client.Close(onClose);
	}};

	// We must call io_context::run for asynchronous callbacks to run
	client.Connect(onConnect, onReceive);
	ioc.run();

  // When we get here, the io_context::run function has run out of work to do
	BOOST_CHECK(connected);
	BOOST_CHECK(messageSent);
	BOOST_CHECK(messageReceived);
	BOOST_CHECK(messageMatches);
	BOOST_CHECK_EQUAL(disconnected, 1);
}

bool CheckResponse(const std::string& response)
{
	bool ok { true };

	ok &= response.find("ERROR") != std::string::npos;
	ok &= response.find("ValidationInvalidAuth") != std::string::npos;

	return ok;
}

BOOST_AUTO_TEST_CASE(send_stomp_frame)
{
	// Connection targets
	const std::string url 						{"ltnm.learncppthroughprojects.com"};
	const std::string endpoint 				{"/network-events"};
	const std::string port 						{"443"};

	// STOMP frame
	const std::string username { "fake_username" };
	const std::string password { "fake_password" };

	std::stringstream stomp_message 	{};
	stomp_message << "STOMP\n"
					<< "accept-version:1.2\n"
					<< "host:ltnm.learncppthroughprojects.com\n"
					<< "login:" << username<< '\n'
					<< "passcode:" << password << '\n'
					<< '\n' // Headers need to be followed by a blank line
					<< '\0'; // The body (even if absent) must be followed by a NULL octet
	const std::string message { stomp_message.str() };
	
	// TLS context
	boost::asio::ssl::context ctx { boost::asio::ssl::context::tlsv12_client };
	ctx.load_verify_file(TESTS_CACERT_PEM);

	// Always start with an I/O context object.
	boost::asio::io_context ioc {};

	// The class under test
	WebSocketClient client { url, endpoint, port, ioc, ctx };

	// We use these flags to check the connection, send, receive functions
	//	work as expected
	bool connected 				{false};
	bool messageSent 			{false};
	bool messageReceived 	{false};
	bool disconnected 		{false};
	std::string response {};

	// Our own callbacks
	auto onSend { [&messageSent](auto ec) {
		messageSent = !ec;
	}};

	auto onConnect { [&client, &connected, &onSend, &message](auto ec) {
		connected = !ec;
		if (!ec)
		{
			client.Send(message, onSend);
		}
	}};

	auto onClose { [&disconnected](auto ec) {
		disconnected = !ec;
	}};

	auto onReceive { [&client,
										&onClose,
										&messageReceived,
										&response](auto ec, auto received) {
		messageReceived = !ec;
		response = std::move(received);
		client.Close(onClose);
	}};

	// We must call io_context::run for asynchronous callbacks to run
	client.Connect(onConnect, onReceive);
	ioc.run();

  // When we get here, the io_context::run function has run out of work to do
	BOOST_CHECK(connected);
	BOOST_CHECK(messageSent);
	BOOST_CHECK(messageReceived);
	BOOST_CHECK(disconnected);
	BOOST_CHECK(CheckResponse(response));
}

BOOST_AUTO_TEST_SUITE_END();
