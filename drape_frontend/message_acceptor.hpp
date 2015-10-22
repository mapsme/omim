#pragma once

#include "drape_frontend/message_queue.hpp"

#include "drape/pointers.hpp"

namespace df
{

class Message;

class MessageAcceptor
{
protected:
  virtual ~MessageAcceptor(){}

  virtual void AcceptMessage(dp::RefPointer<Message> message) = 0;
  virtual bool CanReceiveMessage() = 0;

  /// Must be called by subclass on message target thread
  void ProcessSingleMessage(unsigned maxTimeWait = -1);

  void CancelMessageWaiting();

  void CloseQueue();

private:
  friend class ThreadsCommutator;

  void PostMessage(dp::TransferPointer<Message> message, MessagePriority priority);

private:
  MessageQueue m_messageQueue;
};

} // namespace df
