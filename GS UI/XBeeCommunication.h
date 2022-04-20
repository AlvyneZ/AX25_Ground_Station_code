/*
   Author: AlvyneZ

   Code for NaSPUoN XBee communication
*/


#ifndef XBEE_COMMUNICATION_H
#define XBEE_COMMUNICATION_H


#include <map>
#include <vector>


#define START_DELIMITER						0x7E



//---------------FRAME TYPES----------------------------------

//#define SIXTY_FOUR_BIT_TRANSMIT_REQUEST	0x00
#define SIXTEEN_BIT_TRANSMIT_REQUEST		0x01
#define LOCAL_AT_COMMAND_REQUEST			0x08
#define QUEUE_LOCAL_AT_COMMAND_REQUEST		0x09
#define TRANSMIT_REQUEST					0x10
//#define EXPLICIT_ADDRESSING_COMMAND_REQUEST	0x11
//#define REMOTE_AT_COMMAND_REQUEST			0x17
//#define BLE_UNLOCK_REQUEST				0x2C
//#define USER_DATA_RELAY_INPUT				0x2D
//#define SECURE_SESSION_CONTROL			0x2E

//#define SIXTY_FOUR_BIT_RECEIVE_PACKET		0x80
#define SIXTEEN_BIT_RECEIVE_PACKET			0x81
//#define SIXTY_FOUR_BIT_IO_SAMPLE_INDICATR	0x82
//#define SIXTEEN_BIT_IO_SAMPLE_INDICATOR	0x83
#define LOCAL_AT_COMMAND_RESPONSE			0x88
#define TRANSMIT_STATUS						0x89
#define MODEM_STATUS						0x8A
#define EXTENDED_TRANSMIT_STATUS			0x8B
#define RECEIVE_PACKET						0x90
#define EXPLICIT_RECEIVE_INDICATOR			0x91
//#define IO_SAMPLE_INDICATOR				0x92
//#define REMOTE_AT_COMMAND_RESPONSE		0x97
//#define EXTENDED_MODEM_STATUS				0x98
//#define BLE_UNLOCK_RESPONSE				0xAC
//#define USER_DATA_RELAY_OUTPUT			0xAD
//#define SECURE_SESSION_RESPONSE			0xAE


namespace commsXBeeNaSPUoN
{
	//To allow the system to correlate a response with a previously sent frame
	std::map< uint8_t, std::vector<uint8_t> > sentFramesXB;

	//Since the UART may read half frames
	std::vector<uint8_t> receivedFrame;

	//For XBee s2c (hardware used for testing which operates in legacy mode)
	bool legacyXBee = true;

	//To store high priority RF frames if they fail after being sent
	std::vector< std::vector<uint8_t> > framesToResend;
}


int GSUI::MyForm::incrementFrameIDCountXB() {
	msclr::lock lck(this->frameIDMutex);
	int ret = this->frameIDCountXB;
	this->frameIDCountXB++;
	if (this->frameIDCountXB == 0x00)
		this->frameIDCountXB++;
	lck.release();
	return ret;
}

uint8_t GSUI::MyForm::ChecksumCalculator(std::vector<uint8_t> & data) {
	//This function calculates the checksum to be appended to the end of a frame
	//The function uses the data to be sent that is just after the length

	uint8_t sum = 0x00;
	for (unsigned int i = 0; i < data.size(); i++) {
		sum += data[i];
	}
	sum = 0xFF - sum;
	return sum;
}

void GSUI::MyForm::FrameConstructorXB(std::vector<uint8_t> & data) {
	//This function constructs the frame that would be forwarded to the XBee
	//The function adds the checksum, frame ID, length and start delimiter

	data.insert((data.begin() + 1), incrementFrameIDCountXB());

	uint16_t length = (uint16_t)(data.size());
	uint8_t len_high = (uint8_t)((length & 0xFF00) >> 8);
	uint8_t len_low = (uint8_t)(length & 0x00FF);
	uint8_t checksum = ChecksumCalculator(data);

	data.insert(data.begin(), len_low);
	data.insert(data.begin(), len_high);
	data.insert(data.begin(), START_DELIMITER);
	data.push_back(checksum);
}

std::vector< std::vector<uint8_t> > GSUI::MyForm::checkReceivedPacketForFramesXB(std::vector<uint8_t> & rx) {
	//This function checks the received packet for valid frames and returns the valid frames in a vector
	//	If a frame was not fully received, it retrieves it

	commsXBeeNaSPUoN::receivedFrame.insert(commsXBeeNaSPUoN::receivedFrame.end(), rx.begin(), rx.end());

	std::vector< std::vector<uint8_t> > foundFrames;
	std::vector<uint8_t> frame;
	uint16_t length;
	while ((commsXBeeNaSPUoN::receivedFrame.size()) >= 3) {
		if (commsXBeeNaSPUoN::receivedFrame[0] == START_DELIMITER) {
			//Beginning of a valid frame
			frame.clear();
			length = ((uint16_t)commsXBeeNaSPUoN::receivedFrame[1] << 8) | (commsXBeeNaSPUoN::receivedFrame[2]);

			if (commsXBeeNaSPUoN::receivedFrame.size() >= (unsigned)(length + 4)) {
				frame.insert(frame.begin(), (commsXBeeNaSPUoN::receivedFrame.begin()), (commsXBeeNaSPUoN::receivedFrame.begin() + length + 4));
				commsXBeeNaSPUoN::receivedFrame.erase((commsXBeeNaSPUoN::receivedFrame.begin()), (commsXBeeNaSPUoN::receivedFrame.begin() + length + 4));
				foundFrames.push_back(frame);
			}
			else {
				return foundFrames;
			}
		}
		else {
			commsXBeeNaSPUoN::receivedFrame.erase(commsXBeeNaSPUoN::receivedFrame.begin());
		}
	}

	return foundFrames;
}

bool GSUI::MyForm::isRFPacketHighPriority(std::vector<uint8_t> frame) {
	//Checks if a packet should be added to the resendVector
	//High priotity packets include: TC RF packets, and nothing else (for now)
	try {
		if (commsXBeeNaSPUoN::legacyXBee) {
			if (frame.size() < 10) //To avoid accessing outside vector size
				return false;
			return ((frame[8] == 'T') && ((frame[9] == 'C') || (frame[9] == 'B') || (frame[9] == 'I')));
		}
		else {
			if (frame.size() < 19) //To avoid accessing outside vector size
				return false;
			return ((frame[17] == 'T') && ((frame[18] == 'C') || (frame[18] == 'B') || (frame[18] == 'I')));
		}
	}
	catch (...) {
		return false;
	}
}

std::vector<uint8_t> GSUI::MyForm::removeFrameFromSentFrames(std::vector<uint8_t> & responseFrame) {
	std::vector<uint8_t> originalFrame;
	if (responseFrame.size() < 5) //To avoid accessing outside vector size
		return originalFrame;
	uint8_t frameID = responseFrame[4];
	if ((responseFrame[3] == TRANSMIT_STATUS) || (responseFrame[3] == EXTENDED_TRANSMIT_STATUS))
		allowUplinkTransferToContinue(frameID);
	if (commsXBeeNaSPUoN::sentFramesXB.count(frameID)) {
		originalFrame = commsXBeeNaSPUoN::sentFramesXB[frameID];
		commsXBeeNaSPUoN::sentFramesXB.erase(frameID);
	}
	else {
		std::string receivedFrame = vectorToHexString(responseFrame);
		logErr("XBComms -> Could not find corresponding request for the following response: " + receivedFrame);
	}
	return originalFrame;
}

void GSUI::MyForm::processReceivedPacketXB(std::vector<uint8_t> & rx) {
	//This function takes received packets and retrieves the valid frames from the packet
	//  then processes the instructions

	std::vector< std::vector<uint8_t> > frames = checkReceivedPacketForFramesXB(rx);
	std::vector<uint8_t> correspondingRequest;

	//Checking for corresponding frameID if the frame is a response frame
	for (unsigned int i = 0; i < frames.size(); i++) {
		correspondingRequest.clear();
		if (frames[i].size() < 4) //To avoid accessing outside vector size
			continue;
		switch (frames[i][3]) {
		case MODEM_STATUS:
		{
			std::string statusMsg;
			if (frames[i].size() < 5) //To avoid accessing outside vector size
				continue;
			switch (frames[i][4]) {
			case 0x00:
				statusMsg = "Hardware reset or power up";
				break;
			case 0x01:
				statusMsg = "Watchdog timer reset";
				break;
			case 0x02:
				statusMsg = "End device successfully associated with a coordinator";
				break;
			case 0x03:
				statusMsg = "End device disassociated from coordinator or coordinator failed to form a new network";
				break;
			case 0x06:
				statusMsg = "Coordinator formed a new network";
				break;
			case 0x0D:
				statusMsg = "Voltage supply limit exceeded";
				break;
			case 0x3B:
				statusMsg = "Secure session successfully established";
				break;
			case 0x3C:
				statusMsg = "Secure session ended";
				break;
			case 0x3D:
				statusMsg = "Secure session authentication failed";
				break;
			case 0x32:
				statusMsg = "BLE Connect";
				break;
			case 0x33:
				statusMsg = "BLE Disconnect";
				break;
			case 0x34:
				statusMsg = "No Secure Session Connection";
				break;
			default:
				statusMsg = "STATUS code: " + integerToHexString(frames[i][4], 2);
			}
			log("XBComms -> Modem Status -> " + statusMsg);
			break;
		}
		case TRANSMIT_STATUS:
		{
			correspondingRequest = removeFrameFromSentFrames(frames[i]);
			if (frames[i].size() < 6) //To avoid accessing outside vector size
				continue;
			if (frames[i][5] != 0x00) {
				if (isRFPacketHighPriority(correspondingRequest)) {
					msclr::lock lck(framesToResendMutex);
					commsXBeeNaSPUoN::framesToResend.push_back(correspondingRequest);
					lck.release();
				}
				std::string errorCause;
				switch (frames[i][5]) {
				case 0x01:
					errorCause = "No ACK received";
					break;
				case 0x02:
					errorCause = "CCA failure";
					break;
				case 0x03:
					errorCause = "Indirect message unrequested";
					break;
				case 0x04:
					errorCause = "Transceiver was unable to complete the transmission";
					break;
				case 0x21:
					errorCause = "Network ACK Failure";
					break;
				case 0x22:
					errorCause = "Not joined to network";
					break;
				case 0x31:
					errorCause = "Internal error";
					break;
				case 0x32:
					errorCause = "Resource error - lack of free buffers, timers, etc.";
					break;
				case 0x34:
					errorCause = "No Secure Session Connection";
					break;
				case 0x35:
					errorCause = "Encryption Failure";
					break;
				case 0x74:
					errorCause = "Message too long";
					break;
				default:
					errorCause = "ERROR code: " + integerToHexString(frames[i][8], 2);
				}
				std::string request = vectorToHexString(correspondingRequest);
				if (request == "") {
					request = " Could not find corresponding Request.";
				}
				logErr("XBComms -> Transmit Error {" + errorCause + "} for the following request:" + request);
			}
			break;
		}
		case EXTENDED_TRANSMIT_STATUS:
		{
			correspondingRequest = removeFrameFromSentFrames(frames[i]);
			if (frames[i].size() < 9) //To avoid accessing outside vector size
				continue;
			//Bytes 5&6 have the 16 bit destination address
			std::string retryCount = std::to_string(frames[1][7]) + " retries ";
			if (frames[i][8] != 0x00) {
				std::string errorCause;
				switch (frames[i][8]) {
				case 0x01:
					errorCause = "MAC ACK Failure";
					break;
				case 0x02:
					errorCause = "CCA failure";
					break;
				case 0x03:
					errorCause = "Indirect message unrequested";
					break;
				case 0x21:
					errorCause = "Network ACK Failure";
					break;
				case 0x31:
					errorCause = "Internal resource error";
					break;
				case 0x34:
					errorCause = "No Secure Session Connection";
					break;
				case 0x35:
					errorCause = "Encryption Failure";
					break;
				case 0x74:
					errorCause = "Data payload too large";
					break;
				default:
					errorCause = "ERROR code: " + integerToHexString(frames[i][8], 2);
				}
				std::string request = vectorToHexString(correspondingRequest);
				if (request == "") {
					request = " Could not find corresponding Request.";
				}
				logErr("XBComms -> Transmit Error {" + errorCause + "} after " + retryCount + "for the following request:" + request);
			}
			break;
		}
		case SIXTEEN_BIT_RECEIVE_PACKET:
		{
			if (frames[i].size() < 9) //To avoid accessing outside vector size
				continue;
			std::vector<uint8_t> sixtyFourBitAddressVector({ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF });
			std::vector<uint8_t> sixteenBitAddressVector(frames[i].begin() + 4, frames[i].begin() + 6);
			uint16_t sixteenBitAddress = makeSixteenBitInt(sixteenBitAddressVector);
			//Byte 6 has RSSI, Byte 7 has Options(bit1-broadcast, bit2-PAN broadcast)
			std::vector<uint8_t> payload(frames[i].begin() + 8, frames[i].end() - 1);
			///log("Received a packet of payload: " + vectorToHexString(payload) + " from 16-bit sender: " + integerToHexString(sixteenBitAddress, 4));
			processIncomingPayload(sixtyFourBitAddressVector, sixteenBitAddress, payload);
			///log("Finished processing the payload");
			break;
		}
		case RECEIVE_PACKET:
		{
			if (frames[i].size() < 16) //To avoid accessing outside vector size
				continue;
			std::vector<uint8_t> sixtyFourBitAddressVector(frames[i].begin() + 4, frames[i].begin() + 12);
			std::vector<uint8_t> sixteenBitAddressVector(frames[i].begin() + 12, frames[i].begin() + 14);
			uint16_t sixteenBitAddress = makeSixteenBitInt(sixteenBitAddressVector);
			//Byte 14 has receive Options
			std::vector<uint8_t> payload(frames[i].begin() + 15, frames[i].end() - 1);
			processIncomingPayload(sixtyFourBitAddressVector, sixteenBitAddress, payload);
			break;
		}
		case EXPLICIT_RECEIVE_INDICATOR:
		{
			if (frames[i].size() < 22) //To avoid accessing outside vector size
				continue;
			std::vector<uint8_t> sixtyFourBitAddressVector(frames[i].begin() + 4, frames[i].begin() + 12);
			std::vector<uint8_t> sixteenBitAddressVector(frames[i].begin() + 12, frames[i].begin() + 14);
			uint16_t sixteenBitAddress = makeSixteenBitInt(sixteenBitAddressVector);
			//Byte 14 has source endpoint, Byte 15 hasdestination endpoint
			//Byte 16&17 have clusterID(16bit), Byte 18&19 have profileID(16bit)
			//Byte 20 has receive Options
			std::vector<uint8_t> payload(frames[i].begin() + 21, frames[i].end() - 1);
			processIncomingPayload(sixtyFourBitAddressVector, sixteenBitAddress, payload);
			break;
		}
		default:
			{
				if (frames[i].size() < 4) //To avoid accessing outside vector size
					continue;
				std::string packet = vectorToHexString(frames[i]);
				std::string frameType = integerToHexString(frames[i][3], 2);
				log("XBComms -> Got an interesting packet from XBee {Frame Type " + frameType + "}:" + packet);
			}
		}
	}
}

void GSUI::MyForm::sendFrameXB(std::vector<uint8_t> & frame, int rightPadding) {
	//This function sends fully structured frames to the XBee
	//The function allows for zeros to be added after the frame for receiving purposes

	commsXBeeNaSPUoN::sentFramesXB.insert(std::make_pair(frame[4], frame));
	if (rightPadding > 0)
		frame.resize(frame.size() + rightPadding, 0x00);
	sendSerial(frame);
}

void GSUI::MyForm::sendRFPacketXB(std::vector<uint8_t> sixtyFourBitAddress, uint16_t sixteenBitAddress, std::vector<uint8_t> & packet) {
	if (commsXBeeNaSPUoN::legacyXBee) {
		packet.insert(packet.begin(), 0x00); //Options
		insertSixteenBitIntInEightBitVector(packet, packet.begin(), sixteenBitAddress);
		packet.insert(packet.begin(), SIXTEEN_BIT_TRANSMIT_REQUEST); //Frame Type
	}
	else {
		packet.insert(packet.begin(), 0x00); //Transmit Options
		packet.insert(packet.begin(), 0x00); //Broadcast Radius
		insertSixteenBitIntInEightBitVector(packet, packet.begin(), sixteenBitAddress);
		packet.insert(packet.begin(), sixtyFourBitAddress.begin(), sixtyFourBitAddress.end());
		packet.insert(packet.begin(), TRANSMIT_REQUEST); //Frame Type
	}

	FrameConstructorXB(packet);

	///log("RF packet to be sent: " + vectorToHexString(packet));

	sendFrameXB(packet, 0);
}

void GSUI::MyForm::resendHighPriorityFrames() {
	msclr::lock lck(framesToResendMutex);
	std::vector< std::vector<uint8_t> > resend = commsXBeeNaSPUoN::framesToResend;
	commsXBeeNaSPUoN::framesToResend.clear();
	lck.release();

	for (int i = 0; i < resend.size(); i++) {
		sendFrameXB(resend[i], 0);
	}
}



#endif