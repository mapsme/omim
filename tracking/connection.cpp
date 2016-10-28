#include "tracking/connection.hpp"

#include "tracking/protocol.hpp"

#include "platform/platform.hpp"
#include "platform/socket.hpp"

namespace
{
uint32_t constexpr kSocketTimeoutMs = 10000;
}  // namespace

namespace tracking
{
Connection::Connection(unique_ptr<platform::Socket> socket, string const & host, uint16_t port,
                       bool isHistorical)
  : m_socket(move(socket)), m_host(host), m_port(port), m_isHistorical(isHistorical)
{
  if (!m_socket)
    return;

  m_socket->SetTimeout(kSocketTimeoutMs);
}

// TODO: implement handshake
bool Connection::Reconnect()
{
  if (!m_socket)
    return false;

  m_socket->Close();

  if (!m_socket->Open(m_host, m_port))
    return false;

  auto packet = Protocol::CreateAuthPacket(GetPlatform().UniqueClientId());
  if (!m_socket->Write(packet.data(), static_cast<uint32_t>(packet.size())))
    return false;

  string check(begin(Protocol::kFail), end(Protocol::kFail));
  bool const isSuccess =
      m_socket->Read(reinterpret_cast<uint8_t *>(&check[0]), static_cast<uint32_t>(check.size()));
  if (!isSuccess || check != string(begin(Protocol::kOk), end(Protocol::kOk)))
    return false;

  return true;
}

// TODO: implement historical
bool Connection::Send(boost::circular_buffer<DataPoint> const & points)
{
  if (!m_socket)
    return false;

  auto packet = Protocol::CreateDataPacket(points);
  return m_socket->Write(packet.data(), static_cast<uint32_t>(packet.size()));
}
}  // namespace tracking
