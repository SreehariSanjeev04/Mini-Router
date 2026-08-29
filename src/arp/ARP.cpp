#include "ARP.h"

namespace ARP {

    /**
     * Parses an ARP message from the given data buffer.
     * @param data Pointer to the data buffer containing the ARP message.
     * @param length Length of the data buffer.
     * @param message Reference to the Message struct where the parsed data will be stored.
     * @return True if parsing was successful, false otherwise.
     */
    bool parse(const uint8_t* data, size_t length, Message& message) {
        if (data == nullptr || length < sizeof(Message)) { // doubtful
            return false;
        }

        size_t offset = 0;
        uint16_t rawHwType, rawProtoType, rawOp;
        std::memcpy(&rawHwType, data + offset, sizeof(rawHwType));
        message.hardware_type = ntohs(rawHwType);
        offset += sizeof(rawHwType);   

        std::memcpy(&rawProtoType, data + offset, sizeof(rawProtoType));
        message.protocol_type = ntohs(rawProtoType);
        offset += sizeof(rawProtoType);

        std::memcpy(&message.hardware_size, data + offset, sizeof(message.hardware_size));
        offset += sizeof(message.hardware_size);

        std::memcpy(&message.protocol_size, data + offset, sizeof(message.protocol_size));
        offset += sizeof(message.protocol_size);

        std::memcpy(&rawOp, data + offset, sizeof(rawOp));
        message.opcode = ntohs(rawOp);
        offset += sizeof(rawOp);

        std::memcpy(message.sender_mac.data(), data + offset, message.sender_mac.size());
        offset += message.sender_mac.size();

        std::memcpy(message.sender_ip.data(), data + offset, message.sender_ip.size());
        offset += message.sender_ip.size();

        std::memcpy(message.target_mac.data(), data + offset, message.target_mac.size());
        offset += message.target_mac.size();

        std::memcpy(message.target_ip.data(), data + offset, message.target_ip.size());
        offset += message.target_ip.size();

        return true;
    }

    /**
     * Serializes an ARP message into the given buffer.
     * @param message The ARP message to serialize.
     * @param buffer Pointer to the buffer where the serialized data will be stored.
     * @param bufferSize Size of the buffer.
     * @return True if serialization was successful, false otherwise.
     */
    bool serialize(const Message& message, uint8_t* buffer, size_t bufferSize) {
        if (bufferSize < sizeof(Message)) {
            return false;
        }

        size_t offset = 0;
        uint16_t rawHwType = htons(message.hardware_type);
        uint16_t rawProtoType = htons(message.protocol_type);
        uint16_t rawOp = htons(message.opcode);

        std::memcpy(buffer + offset, &rawHwType, sizeof(rawHwType));
        offset += sizeof(rawHwType);
        std::memcpy(buffer + offset, &rawProtoType, sizeof(rawProtoType));
        offset += sizeof(rawProtoType);
        std::memcpy(buffer + offset, &message.hardware_size, sizeof(message.hardware_size));
        offset += sizeof(message.hardware_size);
        std::memcpy(buffer + offset, &message.protocol_size, sizeof(message.protocol_size));
        offset += sizeof(message.protocol_size);
        std::memcpy(buffer + offset, &rawOp, sizeof(rawOp));
        offset += sizeof(rawOp);
        
        std::memcpy(buffer + offset, message.sender_mac.data(), message.sender_mac.size());
        offset += message.sender_mac.size();
        
        std::memcpy(buffer + offset, message.sender_ip.data(), message.sender_ip.size());
        offset += message.sender_ip.size();

        std::memcpy(buffer + offset, message.target_mac.data(), message.target_mac.size());
        offset += message.target_mac.size();

        std::memcpy(buffer + offset, message.target_ip.data(), message.target_ip.size());
        offset += message.target_ip.size();

        return true;
    }

    /**
     * Checks if the given ARP message is a request.
     * @param message The ARP message to check.
     * @return True if the message is a request, false otherwise.
     */
    bool isRequest(const Message& message) {
        return message.opcode == static_cast<uint16_t>(Net::ARP::Opcode::Request);
    }

    /**
     * Checks if the given ARP message is a reply.
     * @param message The ARP message to check.
     * @return True if the message is a reply, false otherwise.
     */
    bool isReply(const Message& message) {
        return message.opcode == static_cast<uint16_t>(Net::ARP::Opcode::Reply);
    }

    /**
     * Validates the given ARP message.
     * @param message The ARP message to validate.
     * @return True if the message is valid, false otherwise.
     */
    bool isValid(const Message& message) {
        return message.hardware_type != 0 && message.protocol_type != 0 && message.hardware_size != 0 && message.protocol_size != 0;
    }

    /**
     * Creates a reply to the given ARP message.
     * @param message The ARP message to reply to.
     * @param senderMac The MAC address of the sender.
     * @param sendIP The IP address of the sender.
     * @return The created reply message.
     */
    Message createReply(
        const Message& message,
        const std::array<uint8_t, 6>& senderMac,
        const std::array<uint8_t, 4>& sendIP
    ) {
        Message reply;
        reply.hardware_type = message.hardware_type;
        reply.protocol_type = message.protocol_type;
        reply.hardware_size = message.hardware_size;
        reply.protocol_size = message.protocol_size;
        reply.opcode = static_cast<uint16_t>(Net::ARP::Opcode::Reply);
        reply.sender_mac = senderMac;
        reply.sender_ip = sendIP;
        reply.target_mac = message.sender_mac;
        reply.target_ip = message.sender_ip;

        return reply;
    }
}