#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/ssl.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <functional>

namespace NetworkMonitor
{
/*! \brief Client to connect to a WebSocket server over plain TCP
  */
class WebSocketClient
{
  public:
    /*! \brief Construct a WebSocket client
      *  
      *  \note This constructor does not initiate a connection
      *  
      *  \param url      The URL of the server
      *  \param endpoint The endpointon the server to connect to
      *                  Example: ltnm.learncppthroughprojects.com/<endpoint>
      *  \param port     The port on the server
      *  \param ioc      The io_context object. The user takes care of calling
      *                  ioc.run()
      *  \param ctx      The TLS context to setup a TLS socket stream
      */
    WebSocketClient(
      const std::string& url,
      const std::string& endpoint,
      const std::string& port,
      boost::asio::io_context& ioc,
      boost::asio::ssl::context& ctx
    );

    /*! \brief Destructor.
      */
    ~WebSocketClient();

    /*! \brief Connect to the server
      *
      *  \param onConnect    Called when the connection fails or succeed
      *  \param onMessage    Called only when a message is successfully
      *                      received. The message is an rvalue reference;
      *                      ownership is passed to the receiver
      *  \param onDisconnect Called when the connection is closed by the server
      *                      or due to a connection error.
      */
    void Connect(
      std::function<void (boost::system::error_code)> onConnect     = nullptr,
      std::function<void (boost::system::error_code,
                          std::string&&)>             onMessage     = nullptr,
      std::function<void (boost::system::error_code)> onDisconnect  = nullptr
    );

    /*! \brief Send a text message to the WebSocket server
      *
      *  \param message  The message to send. The caller must ensure that this
      *                  string stays in scope until the onSend handler is called
      *  \param onSend   Called when a message is sent successfully or if it
      *                  failed to send  
      */
    void Send(
      const std::string& message,
      std::function<void (boost::system::error_code)> onSend = nullptr
    );

    /*! \brief Close the WebSocket connection
      *
      *  \param onClose  Called when the connection is closed, successfully or
      *                  not
      */
    void Close(
      std::function<void (boost::system::error_code)> onClose = nullptr
    );

  private:
    std::string m_url{};
    std::string m_endpoint{};
    std::string m_port{};

    // We leave these uninitialized because they do not support a default
    //  constructor
    boost::asio::ip::tcp::resolver m_resolver;
    boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::beast::tcp_stream>
    > m_ws;
    boost::beast::flat_buffer m_rBuffer{};

    bool m_closed { true };

    std::function<void (boost::system::error_code)> m_onConnect {nullptr};
    std::function<void (boost::system::error_code,
                        std::string&&)> m_onMessage {nullptr};
    std::function<void (boost::system::error_code)> m_onDisconnect {nullptr};

    void OnResolve(
        const boost::system::error_code& ec,
        boost::asio::ip::tcp::resolver::iterator resolverIt
    );

    void OnConnect(
        const boost::system::error_code& ec
    );

    void OnHandshake(
        const boost::system::error_code& ec
    );

    void ListenToIncomingMessage(
        const boost::system::error_code& ec
    );

    void OnRead(
        const boost::system::error_code& ec,
        size_t nBytes
    );

    void OnTlsHandshake(
      const boost::system::error_code& ec
    );

};

} // namespace NetworkMonitor

#endif
