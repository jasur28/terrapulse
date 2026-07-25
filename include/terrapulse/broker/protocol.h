#pragma once

namespace tp::broker::protocol {

inline constexpr const char* kCommandConnect = "CONNECT";
inline constexpr const char* kCommandDisconnect = "DISCONNECT";
inline constexpr const char* kCommandSubscribe = "SUBSCRIBE";
inline constexpr const char* kCommandUnsubscribe = "UNSUBSCRIBE";
inline constexpr const char* kCommandSend = "SEND";
inline constexpr const char* kCommandState = "STATE";

inline constexpr const char* kReplyConnected = "CONNECTED";
inline constexpr const char* kReplyReceive = "RECV";
inline constexpr const char* kReplyAck = "ACK";
inline constexpr const char* kReplyReceipt = "RECEIPT";
inline constexpr const char* kReplyEnter = "ENTER";
inline constexpr const char* kReplyLeave = "LEAVE";
inline constexpr const char* kReplyState = "STATE";
inline constexpr const char* kReplyDisconnected = "DISCONNECTED";
inline constexpr const char* kReplyError = "ERROR";

inline constexpr const char* kHeaderQueue = "Queue";
inline constexpr const char* kHeaderClientName = "Client-Name";
inline constexpr const char* kHeaderMembershipInfo = "Membership-Info";
inline constexpr const char* kHeaderSelfDiscard = "Self-Discard";
inline constexpr const char* kHeaderStatusOnly = "Status-Only";
inline constexpr const char* kHeaderAckWindow = "Ack-Window";
inline constexpr const char* kHeaderSeqNo = "Seq-No";
inline constexpr const char* kHeaderSubscriptions = "Subscriptions";
inline constexpr const char* kHeaderDestination = "D";
inline constexpr const char* kHeaderSender = "C";
inline constexpr const char* kHeaderMimeType = "T";
inline constexpr const char* kHeaderEncoding = "E";
inline constexpr const char* kHeaderContentLength = "L";

} // namespace tp::broker::protocol
