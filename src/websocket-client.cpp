#include <network-monitor/websocket-client.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

using NetworkMonitor::WebSocketClient;

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// Static functions

static void Log(const std::string& where, boost::system::error_code ec)
{
  std::cerr << "[" << std::setw(20) << where << "]"
            << (ec ? "Error: " : "OK")
            << (ec ? ec.message() : "")
            << '\n';
}

// Public methods

WebSocketClient::WebSocketClient(
    const std::string& url,
    const std::string& endpoint,
    const std::string& port,
    boost::asio::io_context& ioc,
    boost::asio::ssl::context& ctx
) : m_url{ url }
  , m_endpoint{ endpoint }
  , m_port{ port }
  , m_resolver{ boost::asio::make_strand(ioc) }
  , m_ws{ boost::asio::make_strand(ioc), ctx }
{
}

WebSocketClient::~WebSocketClient() = default;

void WebSocketClient::Connect(
  std::function<void (boost::system::error_code)> onConnect,
  std::function<void (boost::system::error_code,
                      std::string&&)>             onMessage,
  std::function<void (boost::system::error_code)> onDisconnect
)
{
  // Save the user callbacks for later us
  m_onConnect    = onConnect;
  m_onMessage    = onMessage;
  m_onDisconnect = onDisconnect;

  // Start the chain of asynchronous callbacks
  m_closed = false;
  m_resolver.async_resolve(m_url, m_port,
    [this](auto ec, auto resolverIt) {
      OnResolve(ec, resolverIt);
    }
  );
}

void WebSocketClient::Send(
  const std::string& message,
  std::function<void (boost::system::error_code)> onSend
)
{
  m_ws.async_write(boost::asio::buffer(message),
    [onSend](auto ec, auto) {
      if (onSend) {
        onSend(ec);
      }
    }
  );
}

void WebSocketClient::Close(
  std::function<void (boost::system::error_code)> onClose
)
{
  m_closed = true;
  m_ws.async_close(
    websocket::close_code::none,
    [onClose](auto ec) {
      if (onClose)
        onClose(ec);
    }
  );
}

// Private methods

void WebSocketClient::OnResolve(
  const boost::system::error_code& ec,
  tcp::resolver::iterator resolverIt
)
{
  if (ec)
  {
    Log("OnResolve", ec);

    if (m_onConnect)
      m_onConnect(ec);

    return;
  }

  // The following timeout only matters for the purpose of connecting to the
  //  TCP socket. We will reset the timeout to a sensible default after we are
  //  connected.
  m_ws.next_layer().next_layer().expires_after(std::chrono::seconds(5));

  // Connect to the TCP socket
  // Instead of constructing the socket and the ws object seperately, the
  //  socket is now embedded in m_ws, and we access it through next_layer()
  m_ws.next_layer().next_layer().async_connect(*resolverIt,
    [this](auto ec) {
      OnConnect(ec);
    }
  );
}

void WebSocketClient::OnConnect(
  const boost::system::error_code& ec
)
{
  if (ec)
  {
    Log("OnConnect", ec);
    if (m_onConnect)
    {
      m_onConnect(ec);
    }
    return;
  }

  // Now that the TCP socket is connected, we can reset the timeout to
  //  whatever Boost.Beast recommends.
  // Note: The TCP layer is the lowest layer (WebSocket -> TLS -> TCP)
  boost::beast::get_lowest_layer(m_ws).expires_never();
  m_ws.set_option(websocket::stream_base::timeout::suggested(
    boost::beast::role_type::client
  ));

  // Some clients require that we set the host name before the TLS handshake
  // or the connection will fail. We use an OpenSSL function for that.
  SSL_set_tlsext_host_name(m_ws.next_layer().native_handle(), m_url.c_str());

  // Attempt a TLS handshake
  // Note: The TLS layer is the next layer (WebSocket -> TLS -> TCP)
  m_ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
    [this](auto ec) {
      OnTlsHandshake(ec);
    }
  );
}

void WebSocketClient::OnHandshake(
  const boost::system::error_code& ec
)
{
  if (ec)
  {
    Log("OnHandshake", ec);

    if (m_onConnect)
      m_onConnect(ec);

    return;
  }

  // Tell the WebSocket object to exchange messages in text format
  m_ws.text(true);

  // Now that we are connected, set up a recursive asynchronous listener to
  //  receive messages
  ListenToIncomingMessage(ec);

  // Dispatch the user callbacj
  // Note: This call is asynchronous and will block the WebSocket strand
  if (m_onConnect)
  {
    m_onConnect(ec);
  }
}

void WebSocketClient::ListenToIncomingMessage(
  const boost::system::error_code& ec
)
{
  // Stop processing messages if the connection has been aborted
  if (ec == boost::asio::error::operation_aborted)
  {
    // We check the closed_ flag to avoid notifying the user twice
    if (m_onDisconnect && !m_closed)
      m_onDisconnect(ec);

    return;
  }

  // Read a message asynchronously. On a successful read, process the message
  //  and recursively call this function again to process the next message.
  m_ws.async_read(m_rBuffer,
    [this](auto ec, auto nBytes) {
      OnRead(ec, nBytes);
      ListenToIncomingMessage(ec);
    }
  );
}

void WebSocketClient::OnRead(
  const boost::system::error_code& ec,
  size_t nBytes
)
{
  // We just ignore messages that failed to read
  if(ec)
    return;

  // Parse the message and forward it to the user callback
  // Note: This call is synchronous and will block the WebSocket strand
  std::string message { boost::beast::buffers_to_string(m_rBuffer.data()) };
  m_rBuffer.consume(nBytes);
  if (m_onMessage)
  {
    m_onMessage(ec, std::move(message));
  }
}

void WebSocketClient::OnTlsHandshake(
  const boost::system::error_code& ec
)
{
  if (ec)
  {
    Log("OnTlsHandshake", ec);
    return;
  }

  // Attempt a WebSocket handshake
  m_ws.async_handshake(m_url, m_endpoint,
    [this](auto ec) {
      OnHandshake(ec);
    }
  );
}
