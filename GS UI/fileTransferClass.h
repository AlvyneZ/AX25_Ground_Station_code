


#ifndef FILE_TRANSFER_CLASS_H
#define FILE_TRANSFER_CLASS_H


#include <vector>
#include <string>


class FileTransfer {
public:
	bool ready;

	uint16_t transferID;
	uint32_t packetsReceived;
	uint32_t fileSize;
	uint32_t expectedPackets;
	std::string fileName;
	std::vector< std::vector<uint8_t> > packets;
	std::vector<bool> receivedPacket;
};

#endif