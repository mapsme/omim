#pragma once

#include "drape_frontend/message.hpp"

#include "drape/pointers.hpp"

#include "base/condition.hpp"

#include "std/condition_variable.hpp"
#include "std/deque.hpp"
#include "std/mutex.hpp"

namespace df
{

//#define DEBUG_MESSAGE_QUEUE

class MessageQueue
{
public:
  MessageQueue();
  ~MessageQueue();

  /// if queue is empty then return NULL
  drape_ptr<Message> PopMessage(bool waitForMessage);
  void PushMessage(drape_ptr<Message> && message, MessagePriority priority);
  void CancelWait();
  void ClearQuery();

#ifdef DEBUG_MESSAGE_QUEUE
  bool IsEmpty() const;
  size_t GetSize() const;
#endif

private:
  void CancelWaitImpl();

  mutable mutex m_mutex;
  condition_variable m_condition;
  bool m_isWaiting;
  using TMessageNode = pair<drape_ptr<Message>, MessagePriority>;
  deque<TMessageNode> m_messages;
};

} // namespace df
