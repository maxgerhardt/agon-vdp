#ifndef VDU_BUFFERED_H
#define VDU_BUFFERED_H

#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>

#include "agon.h"
#include "buffers.h"
#include "buffer_stream.h"
#include "multi_buffer_stream.h"
#include "sprites.h"
#include "types.h"

// VDU 23, 0, &A0, bufferId; command: Buffered command support
//
void VDUStreamProcessor::vdu_sys_buffered() {
	auto bufferId = readWord_t(); if (bufferId == -1) return;
	auto command = readByte_t(); if (command == -1) return;

	switch (command) {
		case BUFFERED_WRITE: {
			auto length = readWord_t(); if (length == -1) return;
			bufferWrite(bufferId, length);
		}	break;
		case BUFFERED_CALL: {
			bufferCall(bufferId, {});
		}	break;
		case BUFFERED_CLEAR: {
			bufferClear(bufferId);
		}	break;
		case BUFFERED_CREATE: {
			auto size = readWord_t(); if (size == -1) return;
			auto buffer = bufferCreate(bufferId, size);	
			if (buffer) {
				// Ensure buffer is empty
				memset(buffer->getBuffer(), 0, size);
			}
		}	break;
		case BUFFERED_SET_OUTPUT: {
			setOutputStream(bufferId);
		}	break;
		case BUFFERED_ADJUST: {
			bufferAdjust(bufferId);
		}	break;
		case BUFFERED_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; 6, <conditional arguments>  : Conditional call
			if (bufferConditional()) {
				bufferCall(bufferId, {});
			}
		}	break;
		case BUFFERED_JUMP: {
			// VDU 23, 0, &A0, bufferId; 7: Jump to buffer
			// a "jump" (without an offset) to buffer 65535 (-1) indicates a "jump to end"
			AdvancedOffset offset = {};
			offset.blockIndex = bufferId == 65535 ? -1 : 0;
			bufferJump(bufferId, offset);
		}	break;
		case BUFFERED_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; 8, <conditional arguments>  : Conditional jump
			if (bufferConditional()) {
				// ensure offset-less jump to buffer 65535 (-1) is treated as a "jump to end"
				AdvancedOffset offset = {};
				offset.blockIndex = bufferId == 65535 ? -1 : 0;
				bufferJump(bufferId, offset);
			}
		}	break;
		case BUFFERED_OFFSET_JUMP: {
			// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte  : Offset jump
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			bufferJump(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_JUMP: {
			// VDU 23, 0, &A0, bufferId; &0A, offset; offsetHighByte, <conditional arguments>  : Conditional jump with offset
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			if (bufferConditional()) {
				bufferJump(bufferId, offset);
			}
		}	break;
		case BUFFERED_OFFSET_CALL: {
			// VDU 23, 0, &A0, bufferId; &0B, offset; offsetHighByte  : Offset call
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			bufferCall(bufferId, offset);
		}	break;
		case BUFFERED_OFFSET_COND_CALL: {
			// VDU 23, 0, &A0, bufferId; &0C, offset; offsetHighByte, <conditional arguments>  : Conditional call with offset
			auto offset = getOffsetFromStream(true); if (offset.blockOffset == -1) return;
			if (bufferConditional()) {
				bufferCall(bufferId, offset);
			}
		}	break;
		case BUFFERED_COPY: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopy(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_CONSOLIDATE: {
			bufferConsolidate(bufferId);
		}	break;
		case BUFFERED_SPLIT: {
			auto length = readWord_t(); if (length == -1) return;
			uint16_t target[] = { (uint16_t)bufferId };
			bufferSplitInto(bufferId, length, target, false);
		}	break;
		case BUFFERED_SPLIT_INTO: {
			auto length = readWord_t(); if (length == -1) return;
			auto targetBufferIds = getBufferIdsFromStream();
			if (targetBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSplitInto(bufferId, length, targetBufferIds, false);
		}	break;
		case BUFFERED_SPLIT_FROM: {
			auto length = readWord_t(); if (length == -1) return;
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSplitInto(bufferId, length, target, true);
		}	break;
		case BUFFERED_SPLIT_BY: {
			auto width = readWord_t(); if (width == -1) return;
			auto chunks = readWord_t(); if (chunks == -1) return;
			uint16_t target[] = { (uint16_t)bufferId };
			bufferSplitByInto(bufferId, width, chunks, target, false);
		}	break;
		case BUFFERED_SPLIT_BY_INTO: {
			auto width = readWord_t(); if (width == -1) return;
			auto targetBufferIds = getBufferIdsFromStream();
			auto chunks = targetBufferIds.size();
			if (chunks == 0) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSplitByInto(bufferId, width, chunks, targetBufferIds, false);
		}	break;
		case BUFFERED_SPLIT_BY_FROM: {
			auto width = readWord_t(); if (width == -1) return;
			auto chunks = readWord_t(); if (chunks == -1) return;
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSplitByInto(bufferId, width, chunks, target, true);
		}	break;
		case BUFFERED_SPREAD_INTO: {
			auto targetBufferIds = getBufferIdsFromStream();
			if (targetBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no target buffer IDs\n\r");
				return;
			}
			bufferSpreadInto(bufferId, targetBufferIds, false);
		}	break;
		case BUFFERED_SPREAD_FROM: {
			auto targetStart = readWord_t(); if (targetStart == -1 || targetStart == 65535) return;
			uint16_t target[] = { (uint16_t)targetStart };
			bufferSpreadInto(bufferId, target, true);
		}	break;
		case BUFFERED_REVERSE_BLOCKS: {
			bufferReverseBlocks(bufferId);
		}	break;
		case BUFFERED_REVERSE: {
			auto options = readByte_t(); if (options == -1) return;
			bufferReverse(bufferId, options);
		}	break;
		case BUFFERED_COPY_REF: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopyRef(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_COPY_AND_CONSOLIDATE: {
			// read list of source buffer IDs
			auto sourceBufferIds = getBufferIdsFromStream();
			if (sourceBufferIds.empty()) {
				debug_log("vdu_sys_buffered: no source buffer IDs\n\r");
				return;
			}
			bufferCopyAndConsolidate(bufferId, sourceBufferIds);
		}	break;
		case BUFFERED_DEBUG_INFO: {
			debug_log("vdu_sys_buffered: buffer %d, %d streams stored\n\r", bufferId, buffers[bufferId].size());
			if (buffers[bufferId].empty()) {
				return;
			}
			// output contents of buffer stream 0
			auto buffer = buffers[bufferId][0];
			auto bufferLength = buffer->size();
			for (auto i = 0; i < bufferLength; i++) {
				auto data = buffer->getBuffer()[i];
				debug_log("%02X ", data);
			}
			debug_log("\n\r");
		}	break;
		default: {
			debug_log("vdu_sys_buffered: unknown command %d, buffer %d\n\r", command, bufferId);
		}	break;
	}
}

// VDU 23, 0, &A0, bufferId; 0, length; data...: store stream into buffer
// This adds a new stream to the given bufferId
// allowing a single bufferId to store multiple streams of data
//
uint32_t VDUStreamProcessor::bufferWrite(uint16_t bufferId, uint32_t length) {
	auto bufferStream = make_shared_psram<BufferStream>(length);

	debug_log("bufferWrite: storing stream into buffer %d, length %d\n\r", bufferId, length);

	auto remaining = readIntoBuffer(bufferStream->getBuffer(), length);
	if (remaining > 0) {
		// NB this discards the data we just read
		debug_log("bufferWrite: timed out write for buffer %d (%d bytes remaining)\n\r", bufferId, remaining);
		return remaining;
	}

	if (bufferId == 65535) {
		// buffer ID of -1 (65535) reserved so we don't store it
		debug_log("bufferWrite: ignoring buffer 65535\n\r");
		return remaining;
	}

	buffers[bufferId].push_back(std::move(bufferStream));
	debug_log("bufferWrite: stored stream in buffer %d, length %d, %d streams stored\n\r", bufferId, length, buffers[bufferId].size());
	return remaining;
}

// VDU 23, 0, &A0, bufferId; 1: Call buffer
// VDU 23, 0, &A0, bufferId; &0B, offset; offsetHighByte  : Offset call
// Processes all commands from the streams stored against the given bufferId
//
void VDUStreamProcessor::bufferCall(uint16_t callBufferId, AdvancedOffset offset) {
	debug_log("bufferCall: buffer %d\n\r", callBufferId);
	auto bufferId = resolveBufferId(callBufferId, id);
	if (bufferId == -1) {
		debug_log("bufferCall: no buffer ID\n\r");
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferCall: buffer %d not found\n\r", bufferId);
		return;
	}
	if (id != 65535 && inputStream->available() == 0) {
		// tail-call optimise - turn the call into a jump
		bufferJump(bufferId, offset);
		return;
	}
	auto &streams = bufferIter->second;
	auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
	if (offset.blockOffset != 0 || offset.blockIndex != 0) {
		multiBufferStream->seekTo(offset.blockOffset, offset.blockIndex);
	}
	auto streamProcessor = make_unique_psram<VDUStreamProcessor>(std::move(multiBufferStream), outputStream, bufferId);
	if (streamProcessor) {
		streamProcessor->processAllAvailable();
	} else {
		debug_log("bufferCall: failed to create stream processor\n\r");
	}
}

// VDU 23, 0, &A0, bufferId; 2: Clear buffer
// Removes all streams stored against the given bufferId
// sending a bufferId of 65535 (i.e. -1) clears all buffers
//
void VDUStreamProcessor::bufferClear(uint16_t bufferId) {
	debug_log("bufferClear: buffer %d\n\r", bufferId);
	if (bufferId == 65535) {
		buffers.clear();
		resetBitmaps();
		resetSamples();
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferClear: buffer %d not found\n\r", bufferId);
		return;
	}
	buffers.erase(bufferIter);
	clearBitmap(bufferId);
	clearSample(bufferId);
	debug_log("bufferClear: cleared buffer %d\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; 3, size; : Create a writeable buffer
// This is used for creating buffers to redirect output to
//
std::shared_ptr<WritableBufferStream> VDUStreamProcessor::bufferCreate(uint16_t bufferId, uint32_t size) {
	if (bufferId == 65535) {
		debug_log("bufferCreate: bufferId %d is reserved\n\r", bufferId);
		return nullptr;
	}
	if (buffers.find(bufferId) != buffers.end()) {
		debug_log("bufferCreate: buffer %d already exists\n\r", bufferId);
		return nullptr;
	}
	auto buffer = make_shared_psram<WritableBufferStream>(size);
	if (!buffer) {
		debug_log("bufferCreate: failed to create buffer %d\n\r", bufferId);
		return nullptr;
	}
	buffers[bufferId].push_back(buffer);
	debug_log("bufferCreate: created buffer %d, size %d\n\r", bufferId, size);
	return buffer;
}

// VDU 23, 0, &A0, bufferId; 4: Set output to buffer
// use an ID of -1 (65535) to clear the output buffer (no output)
// use an ID of 0 to reset the output buffer to it's original value
//
// TODO add a variant/command to adjust offset inside output stream
void VDUStreamProcessor::setOutputStream(uint16_t bufferId) {
	if (bufferId == 65535) {
		outputStream = nullptr;
		return;
	}
	// bufferId of 0 resets output buffer to it's original value
	// which will usually be the z80 serial port
	if (bufferId == 0) {
		outputStream = originalOutputStream;
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end() || bufferIter->second.empty()) {
		debug_log("setOutputStream: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &output = bufferIter->second.front();
	if (output->isWritable()) {
		outputStream = output;
	} else {
		debug_log("setOutputStream: buffer %d is not writable\n\r", bufferId);
	}
}

// Utility call to read offset from stream, supporting advanced offsets
VDUStreamProcessor::AdvancedOffset VDUStreamProcessor::getOffsetFromStream(bool isAdvanced) {
	AdvancedOffset offset = {};
	if (isAdvanced) {
		offset.blockOffset = read24_t();
		if (offset.blockOffset != -1 && offset.blockOffset & 0x00800000) {
			// top bit of 24-bit offset is set, so we have a block index too
			auto blockIndex = readWord_t();
			if (blockIndex == -1) {
				offset.blockOffset = -1;
			} else {
				offset.blockOffset &= 0x007FFFFF;
				offset.blockIndex = blockIndex;
			}
		}
	} else {
		offset.blockOffset = readWord_t();
	}
	return offset;
}

// Utility call to read a sequence of buffer IDs from the stream
std::vector<uint16_t> VDUStreamProcessor::getBufferIdsFromStream() {
	// read buffer IDs until we get a 65535 (end of list) or a timeout
	std::vector<uint16_t> bufferIds;
	uint32_t bufferId;
	bool looping = true;
	while (looping) {
		bufferId = readWord_t();
		looping = (bufferId != 65535 && bufferId != -1);
		if (looping) {
			bufferIds.push_back(bufferId);
		}
	}
	if (bufferId == -1) {
		// timeout
		bufferIds.clear();
	}

	return bufferIds;
}

// Utility call to read a byte from a buffer at the given offset
int16_t VDUStreamProcessor::getBufferByte(const std::vector<std::shared_ptr<BufferStream>> &buffer, AdvancedOffset &offset, bool iterate) {
	// if offset exceeds the block size, loop to find the correct block
	while (offset.blockIndex < buffer.size() && offset.blockOffset >= buffer[offset.blockIndex]->size()) {
		offset.blockOffset -= buffer[offset.blockIndex]->size();
		offset.blockIndex++;
	}
	if (offset.blockIndex >= buffer.size()) {
		// offset not found in buffer
		return -1;
	}
	auto value = buffer[offset.blockIndex]->getBuffer()[offset.blockOffset];
	if (iterate) {
		offset.blockOffset++;
	}
	return value;
}

// Utility call to set a byte in a buffer at the given offset
bool VDUStreamProcessor::setBufferByte(uint8_t value, const std::vector<std::shared_ptr<BufferStream>> &buffer, AdvancedOffset &offset, bool iterate) {
	// if offset exceeds the block size, loop to find the correct block
	while (offset.blockIndex < buffer.size() && offset.blockOffset >= buffer[offset.blockIndex]->size()) {
		offset.blockOffset -= buffer[offset.blockIndex]->size();
		offset.blockIndex++;
	}
	if (offset.blockIndex >= buffer.size()) {
		// offset not found in buffer
		return false;
	}
	buffer[offset.blockIndex]->getBuffer()[offset.blockOffset] = value;
	if (iterate) {
		offset.blockOffset++;
	}
	return true;
}

// VDU 23, 0, &A0, bufferId; 5, operation, offset; [count;] [operand]: Adjust buffer
// This is used for adjusting the contents of a buffer
// It can be used to overwrite bytes, insert bytes, increment bytes, etc
// Basic operation are not, neg, set, add, add-with-carry, and, or, xor
// Upper bits of operation byte are used to indicate:
// - whether to use a long offset (24-bit) or short offset (16-bit)
// - whether the operand is a buffer-originated value or an immediate value
// - whether to adjust a single target or multiple targets
// - whether to use a single operand or multiple operands
//
void VDUStreamProcessor::bufferAdjust(uint16_t adjustBufferId) {
	auto command = readByte_t();

	bool useAdvancedOffsets = command & ADJUST_ADVANCED_OFFSETS;
	bool useBufferValue = command & ADJUST_BUFFER_VALUE;
	bool useMultiTarget = command & ADJUST_MULTI_TARGET;
	bool useMultiOperand = command & ADJUST_MULTI_OPERAND;
	uint8_t op = command & ADJUST_OP_MASK;
	// Operators that are greater than NEG have an operand value
	bool hasOperand = op > ADJUST_NEG;

	auto offset = getOffsetFromStream(useAdvancedOffsets);
	const std::vector<std::shared_ptr<BufferStream>> * operandBuffer = nullptr;
	auto operandBufferId = 0;
	AdvancedOffset operandOffset = {};
	auto count = 1;

	if (useMultiTarget || useMultiOperand) {
		count = useAdvancedOffsets ? read24_t() : readWord_t();
	}
	if (useBufferValue && hasOperand) {
		operandBufferId = resolveBufferId(readWord_t(), id);
		operandOffset = getOffsetFromStream(useAdvancedOffsets);
		if (operandBufferId == -1) {
			debug_log("bufferAdjust: no operand buffer ID\n\r");
			return;
		}
		auto operandBufferIter = buffers.find(operandBufferId);
		if (operandBufferIter == buffers.end()) {
			debug_log("bufferAdjust: buffer %d not found\n\r", operandBufferId);
			return;
		}
		operandBuffer = &operandBufferIter->second;
	}

	auto bufferId = resolveBufferId(adjustBufferId, id);
	if (bufferId == -1) {
		debug_log("bufferAdjust: no target buffer ID\n\r");
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferAdjust: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;

	if (command == -1 || count == -1 || offset.blockOffset == -1 || operandOffset.blockOffset == -1) {
		debug_log("bufferAdjust: invalid command, count, offset or operand value\n\r");
		return;
	}

	auto sourceValue = 0;
	auto operandValue = 0;
	auto carryValue = 0;
	bool usingCarry = false;

	// if useMultiTarget is set, the we're updating multiple source values
	// if useMultiOperand is also set, we get multiple operand values
	// so...
	// if both useMultiTarget and useMultiOperand are false we're updating a single source value with a single operand
	// if useMultiTarget is false and useMultiOperand is true we're adding all operand values to the same source value
	// if useMultiTarget is true and useMultiOperand is false we're adding the same operand to all source values
	// if both useMultiTarget and useMultiOperand are true we're adding each operand value to the corresponding source value

	if (!useMultiTarget) {
		// we have a singular source value
		sourceValue = getBufferByte(buffer, offset);
	}
	if (hasOperand && !useMultiOperand) {
		// we have a singular operand value
		operandValue = operandBuffer ? getBufferByte(*operandBuffer, operandOffset) : readByte_t();
	}

	debug_log("bufferAdjust: command %d, offset %d, count %d, operandBufferId %d, operandOffset %d, sourceValue %d, operandValue %d\n\r", command, offset, count, operandBufferId, operandOffset, sourceValue, operandValue);
	debug_log("useMultiTarget %d, useMultiOperand %d, useAdvancedOffsets %d, useBufferValue %d\n\r", useMultiTarget, useMultiOperand, useAdvancedOffsets, useBufferValue);

	for (auto i = 0; i < count; i++) {
		if (useMultiTarget) {
			// multiple source values will change
			sourceValue = getBufferByte(buffer, offset);
		}
		if (hasOperand && useMultiOperand) {
			operandValue = operandBuffer ? getBufferByte(*operandBuffer, operandOffset, true) : readByte_t();
		}
		if (sourceValue == -1 || operandValue == -1) {
			debug_log("bufferAdjust: invalid source or operand value\n\r");
			return;
		}

		switch (op) {
			case ADJUST_NOT: {
				sourceValue = ~sourceValue;
			}	break;
			case ADJUST_NEG: {
				sourceValue = -sourceValue;
			}	break;
			case ADJUST_SET: {
				sourceValue = operandValue;
			}	break;
			case ADJUST_ADD: {
				// byte-wise add - no carry, so bytes may overflow
				sourceValue = sourceValue + operandValue;
			}	break;
			case ADJUST_ADD_CARRY: {
				// byte-wise add with carry
				// bytes are treated as being in little-endian order
				usingCarry = true;
				sourceValue = sourceValue + operandValue + carryValue;
				if (sourceValue > 255) {
					carryValue = 1;
					sourceValue -= 256;
				} else {
					carryValue = 0;
				}
			}	break;
			case ADJUST_AND: {
				sourceValue = sourceValue & operandValue;
			}	break;
			case ADJUST_OR: {
				sourceValue = sourceValue | operandValue;
			}	break;
			case ADJUST_XOR: {
				sourceValue = sourceValue ^ operandValue;
			}	break;
		}

		if (useMultiTarget) {
			// multiple source/target values updating, so store inside loop
			if (!setBufferByte(sourceValue, buffer, offset, true)) {
				debug_log("bufferAdjust: failed to set result %d at offset %d:%d\n\r", sourceValue, (int)offset.blockIndex, offset.blockOffset);
				return;
			}
		}
	}
	if (!useMultiTarget) {
		// single source/target value updating, so store outside loop
		// also increment offset in case carry is used
		if (!setBufferByte(sourceValue, buffer, offset, true)) {
			debug_log("bufferAdjust: failed to set result %d at offset %d:%d\n\r", sourceValue, (int)offset.blockIndex, offset.blockOffset);
			return;
		}
	}
	if (usingCarry) {
		// if we were using carry, store the final carry value
		if (!setBufferByte(carryValue, buffer, offset)) {
			debug_log("bufferAdjust: failed to set carry value %d at offset %d:%d\n\r", carryValue, (int)offset.blockIndex, offset.blockOffset);
			return;
		}
	}

	debug_log("bufferAdjust: result %d\n\r", sourceValue);
}

// returns true or false depending on whether conditions are met
// Will read the following arguments from the stream
// operation, checkBufferId; offset; [operand]
// This works in a similar manner to bufferAdjust
// for now, this only supports single-byte comparisons
// as multi-byte comparisons are a bit more complex
//
bool VDUStreamProcessor::bufferConditional() {
	auto command = readByte_t();
	auto checkBufferId = resolveBufferId(readWord_t(), id);

	bool useAdvancedOffsets = command & COND_ADVANCED_OFFSETS;
	bool useBufferValue = command & COND_BUFFER_VALUE;
	uint8_t op = command & COND_OP_MASK;
	// conditional operators that are greater than NOT_EXISTS require an operand
	bool hasOperand = op > COND_NOT_EXISTS;

	auto offset = getOffsetFromStream(useAdvancedOffsets);
	const std::vector<std::shared_ptr<BufferStream>> * operandBuffer = nullptr;
	auto operandBufferId = 0;
	AdvancedOffset operandOffset = {};

	if (useBufferValue && hasOperand) {
		operandBufferId = resolveBufferId(readWord_t(), id);
		operandOffset = getOffsetFromStream(useAdvancedOffsets);
		if (operandBufferId == -1) {
			debug_log("bufferConditional: no operand buffer ID\n\r");
			return false;
		}
		auto operandBufferIter = buffers.find(operandBufferId);
		if (operandBufferIter == buffers.end()) {
			debug_log("bufferConditional: buffer %d not found\n\r", operandBufferId);
			return false;
		}
		operandBuffer = &operandBufferIter->second;
	}

	if (command == -1 || checkBufferId == -1 || offset.blockOffset == -1 || operandOffset.blockOffset == -1) {
		debug_log("bufferConditional: invalid command, checkBufferId, offset or operand value\n\r");
		return false;
	}

	auto checkBufferIter = buffers.find(checkBufferId);
	if (checkBufferIter == buffers.end()) {
		debug_log("bufferConditional: buffer %d not found\n\r", checkBufferId);
		return false;
	}
	auto &checkBuffer = checkBufferIter->second;
	auto sourceValue = getBufferByte(checkBuffer, offset);
	int16_t operandValue = 0;
	if (hasOperand) {
		operandValue = operandBuffer ? getBufferByte(*operandBuffer, operandOffset) : readByte_t();
	}

	debug_log("bufferConditional: command %d, checkBufferId %d, offset %d:%d, operandBufferId %d, operandOffset %d:%d, sourceValue %d, operandValue %d\n\r",
		command, checkBufferId, (int)offset.blockIndex, offset.blockOffset, operandBufferId, (int)operandOffset.blockIndex, operandOffset.blockOffset, sourceValue, operandValue);

	if (sourceValue == -1 || operandValue == -1) {
		debug_log("bufferConditional: invalid source or operand value\n\r");
		return false;
	}

	bool shouldCall = false;

	switch (op) {
		case COND_EXISTS: {
			shouldCall = sourceValue != 0;
		}	break;
		case COND_NOT_EXISTS: {
			shouldCall = sourceValue == 0;
		}	break;
		case COND_EQUAL: {
			shouldCall = sourceValue == operandValue;
		}	break;
		case COND_NOT_EQUAL: {
			shouldCall = sourceValue != operandValue;
		}	break;
		case COND_LESS: {
			shouldCall = sourceValue < operandValue;
		}	break;
		case COND_GREATER: {
			shouldCall = sourceValue > operandValue;
		}	break;
		case COND_LESS_EQUAL: {
			shouldCall = sourceValue <= operandValue;
		}	break;
		case COND_GREATER_EQUAL: {
			shouldCall = sourceValue >= operandValue;
		}	break;
		case COND_AND: {
			shouldCall = sourceValue && operandValue;
		}	break;
		case COND_OR: {
			shouldCall = sourceValue || operandValue;
		}	break;
	}

	debug_log("bufferConditional: evaluated as %s\n\r", shouldCall ? "true" : "false");

	return shouldCall;
}

// VDU 23, 0, &A0, bufferId; 7: Jump to a buffer
// VDU 23, 0, &A0, bufferId; 9, offset; offsetHighByte : Jump to (advanced) offset within buffer
// Change execution to given buffer (from beginning or at an offset)
//
void VDUStreamProcessor::bufferJump(uint16_t bufferId, AdvancedOffset offset) {
	debug_log("bufferJump: buffer %d\n\r", bufferId);
	if (id == 65535) {
		// we're currently the top-level stream, so we can't jump
		// so have to call instead
		return bufferCall(bufferId, offset);
	}
	if (bufferId == 65535 || bufferId == id) {
		// a buffer ID of 65535 is used to indicate current buffer, so we seek to offset
		auto instream = (MultiBufferStream *)inputStream.get();
		instream->seekTo(offset.blockOffset, offset.blockIndex);
		return;
	}
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferJump: buffer %d not found\n\r", bufferId);
		return;
	}
	// replace our input stream with a new one
	auto &streams = bufferIter->second;
	auto multiBufferStream = make_shared_psram<MultiBufferStream>(streams);
	if (offset.blockOffset != 0 || offset.blockIndex != 0) {
		multiBufferStream->seekTo(offset.blockOffset, offset.blockIndex);
	}
	inputStream = std::move(multiBufferStream);
}

// VDU 23, 0, &A0, bufferId; &0D, sourceBufferId; sourceBufferId; ...; 65535; : Copy blocks from buffers
// Copy (blocks from) a list of buffers into a new buffer
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one
// This is useful to construct a single buffer from multiple buffers
// which can be used to construct more complex commands
// Target buffer ID can be included in the source list
//
void VDUStreamProcessor::bufferCopy(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopy: ignoring buffer %d\n\r", bufferId);
		return;
	}
	// prepare a vector for storing our buffers
	std::vector<std::shared_ptr<BufferStream>, psram_allocator<std::shared_ptr<BufferStream>>> streams;
	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			// loop thru blocks stored against this ID
			for (const auto &block : sourceBufferIter->second) {
				// push a copy of the block into our vector
				auto bufferStream = make_shared_psram<BufferStream>(block->size());
				if (!bufferStream || !bufferStream->getBuffer()) {
					debug_log("bufferCopy: failed to create buffer\n\r");
					return;
				}
				debug_log("bufferCopy: copying stream %d bytes\n\r", block->size());
				bufferStream->writeBuffer(block->getBuffer(), block->size());
				streams.push_back(std::move(bufferStream));
			}
		} else {
			debug_log("bufferCopy: buffer %d not found\n\r", sourceId);
		}
	}
	// replace buffer with new one
	buffers[bufferId].assign(std::make_move_iterator(streams.begin()), std::make_move_iterator(streams.end()));
	debug_log("bufferCopy: copied %d streams into buffer %d (%d)\n\r", streams.size(), bufferId, buffers[bufferId].size());
}

// VDU 23, 0, &A0, bufferId; &0E : Consolidate blocks within buffer
// Consolidate multiple streams/blocks into a single block
// This is useful for using bitmaps sent in multiple blocks
//
void VDUStreamProcessor::bufferConsolidate(uint16_t bufferId) {
	// Create a new stream big enough to contain all streams in the given buffer
	// Copy all streams into the new stream
	// Replace the given buffer with the new stream
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferConsolidate: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	if (buffer.size() == 1) {
		// only one stream, so nothing to consolidate
		return;
	}
	// buffer ID exists
	auto bufferStream = consolidateBuffers(buffer);
	if (!bufferStream) {
		debug_log("bufferConsolidate: failed to create buffer\n\r");
		return;
	}
	buffer.clear();
	buffer.push_back(std::move(bufferStream));
	debug_log("bufferConsolidate: consolidated %d streams into buffer %d\n\r", buffer.size(), bufferId);
}

void clearTarget(uint16_t target) {
	auto bufferIter = buffers.find(target);
	if (bufferIter != buffers.end()) {
		bufferIter->second.clear();
	}
	clearBitmap(target);
}

void clearTargets(tcb::span<const uint16_t> targets) {
	for (const auto target : targets) {
		clearTarget(target);
	}
}

// VDU 23, 0, &A0, bufferId; &0F, length; : Split buffer into blocks by length
// VDU 23, 0, &A0, bufferId; &10, length; <bufferIds>; 65535; : Split buffer by length to new buffers
// VDU 23, 0, &A0, bufferId; &11, length; targetBufferId; : Split buffer by length to new buffers from target onwards
// Split a buffer into multiple blocks/streams to new buffers
// Will overwrite any existing buffers
//
void VDUStreamProcessor::bufferSplitInto(uint16_t bufferId, uint16_t length, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSplitInto: buffer %d not found\n\r", bufferId);
		return;
	}
	// get a consolidated version of the buffer
	auto bufferStream = consolidateBuffers(bufferIter->second);
	if (!bufferStream) {
		debug_log("bufferSplitInto: failed to create buffer\n\r");
		return;
	}
	if (!iterate) {
		clearTargets(newBufferIds);
	}

	auto chunks = splitBuffer(std::move(bufferStream), length);
	if (chunks.empty()) {
		debug_log("bufferSplitInto: failed to split buffer\n\r");
		return;
	}
	// distribute our chunks to destination buffers
	auto targetIter = newBufferIds.begin();
	for (auto &chunk : chunks) {
		auto targetId = *targetIter;
		if (iterate) {
			clearTarget(targetId);
		}
		buffers[targetId].push_back(std::move(chunk));
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}
	debug_log("bufferSplitInto: split buffer %d into %d blocks of length %d\n\r", bufferId, chunks.size(), length);
}

// VDU 23, 0, &A0, bufferId; &12, width; chunkCount; : Split buffer by width (in-place)
// VDU 23, 0, &A0, bufferId; &13, width; <bufferIds>; 65535; : Split buffer by width to new buffers
// VDU 23, 0, &A0, bufferId; &14, width; chunkCount; targetBufferId; : Split buffer by width to new buffers from ID onwards
// Split a buffer into multiple blocks/streams to new buffers/chunks by width
// Will overwrite any existing buffers
//
void VDUStreamProcessor::bufferSplitByInto(uint16_t bufferId, uint16_t width, uint16_t chunkCount, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSplitByInto: buffer %d not found\n\r", bufferId);
		return;
	}
	// get a consolidated version of the buffer
	auto bufferStream = consolidateBuffers(bufferIter->second);
	if (!bufferStream) {
		debug_log("bufferSplitByInto: failed to create buffer\n\r");
		return;
	}
	if (!iterate) {
		clearTargets(newBufferIds);
	}

	std::vector<std::vector<std::shared_ptr<BufferStream>>> chunks;
	chunks.resize(chunkCount);
	{
		// split to get raw chunks
		auto rawchunks = splitBuffer(std::move(bufferStream), width);
		if (rawchunks.empty()) {
			debug_log("bufferSplitByInto: failed to split buffer\n\r");
			return;
		}
		// and re-jig into our chunks vector
		auto chunkIndex = 0;
		for (auto &chunk : rawchunks) {
			chunks[chunkIndex].push_back(std::move(chunk));
			chunkIndex++;
			if (chunkIndex >= chunkCount) {
				chunkIndex = 0;
			}
		}
	}

	// consolidate our chunks, and distribute to buffers
	auto targetIter = newBufferIds.begin();
	for (auto &stream : chunks) {
		auto targetId = *targetIter;
		if (iterate) {
			clearTarget(targetId);
		}
		auto chunk = consolidateBuffers(stream);
		if (!chunk) {
			debug_log("bufferSplitByInto: failed to create buffer\n\r");
			return;
		}
		buffers[targetId].push_back(std::move(chunk));
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}

	debug_log("bufferSplitByInto: split buffer %d into %d chunks of width %d\n\r", bufferId, chunkCount, width);
}

// VDU 23, 0, &A0, bufferId; &15, <bufferIds>; 65535; : Spread blocks from buffer into new buffers
// VDU 23, 0, &A0, bufferId; &16, targetBufferId; : Spread blocks from target buffer onwards
//
void VDUStreamProcessor::bufferSpreadInto(uint16_t bufferId, tcb::span<uint16_t> newBufferIds, bool iterate) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferSpreadInto: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	if (!iterate) {
		clearTargets(newBufferIds);
	}
	// iterate over its blocks and send to targets
	auto targetIter = newBufferIds.begin();
	for (const auto &block : buffer) {
		auto targetId = *targetIter;
		if (iterate) {
			clearTarget(targetId);
		}
		buffers[targetId].push_back(block);
		iterate = updateTarget(newBufferIds, targetIter, iterate);
	}
}

// VDU 23, 0, &A0, bufferId; &17 : Reverse blocks within buffer
// Reverses the order of blocks within a buffer
// may be useful for mirroring bitmaps if they have been split by row
//
void VDUStreamProcessor::bufferReverseBlocks(uint16_t bufferId) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter != buffers.end()) {
		// reverse the order of the streams
		auto &buffer = bufferIter->second;
		std::reverse(buffer.begin(), buffer.end());
		debug_log("bufferReverseBlocks: reversed blocks in buffer %d\n\r", bufferId);
	}
}

// VDU 23, 0, &A0, bufferId; &18, options, <parameters> : Reverse buffer
// Reverses the contents of blocks within a buffer
// may be useful for mirroring bitmaps
//
void VDUStreamProcessor::bufferReverse(uint16_t bufferId, uint8_t options) {
	auto bufferIter = buffers.find(bufferId);
	if (bufferIter == buffers.end()) {
		debug_log("bufferReverse: buffer %d not found\n\r", bufferId);
		return;
	}
	auto &buffer = bufferIter->second;
	bool use16Bit = options & REVERSE_16BIT;
	bool use32Bit = options & REVERSE_32BIT;
	bool useSize  = (options & REVERSE_SIZE) == REVERSE_SIZE;
	bool useChunks = options & REVERSE_CHUNKED;
	bool reverseBlocks = options & REVERSE_BLOCK;
	uint8_t unused = options & REVERSE_UNUSED_BITS;

	// future expansion may include:
	// reverse at an offset for a set length (within blocks)
	// reversing across whole buffer (not per block)

	if (unused != 0) {
		debug_log("bufferReverse: warning - unused bits in options byte\n\r");
	}

	auto valueSize = 1;
	auto chunkSize = 0;

	if (useSize) {
		// size follows as a word
		valueSize = readWord_t();
		if (valueSize == -1) {
			return;
		}
	} else if (use32Bit) {
		valueSize = 4;
	} else if (use16Bit) {
		valueSize = 2;
	}

	if (useChunks) {
		chunkSize = readWord_t();
		if (chunkSize == -1) {
			return;
		}
	}

	// verify that our blocks are a multiple of valueSize
	for (const auto &block : buffer) {
		auto size = block->size();
		if (size % valueSize != 0 || (chunkSize != 0 && size % chunkSize != 0)) {
			debug_log("bufferReverse: error - buffer %d contains block not a multiple of value/chunk size %d\n\r", bufferId, valueSize);
			return;
		}
	}

	debug_log("bufferReverse: reversing buffer %d, value size %d, chunk size %d\n\r", bufferId, valueSize, chunkSize);

	for (const auto &block : buffer) {
		if (chunkSize == 0) {
			// no chunking, so simpler reverse
			reverseValues(block->getBuffer(), block->size(), valueSize);
		} else {
			// reverse in chunks
			auto data = block->getBuffer();
			auto chunkCount = block->size() / chunkSize;
			for (auto i = 0; i < chunkCount; i++) {
				reverseValues(data + (i * chunkSize), chunkSize, valueSize);
			}
		}
	}

	if (reverseBlocks) {
		// reverse the order of the streams
		std::reverse(buffer.begin(), buffer.end());
		debug_log("bufferReverse: reversed blocks in buffer %d\n\r", bufferId);
	}

	debug_log("bufferReverse: reversed buffer %d\n\r", bufferId);
}

// VDU 23, 0, &A0, bufferId; &19, sourceBufferId; sourceBufferId; ...; 65535; : Copy references to blocks from buffers
// Copy references to (blocks from) a list of buffers into a new buffer
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one
// This is useful to construct a single buffer from multiple buffers without the copy overhead
// If target buffer is included in the source list it will be skipped to prevent a reference loop
//
void VDUStreamProcessor::bufferCopyRef(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopyRef: ignoring buffer %d\n\r", bufferId);
		return;
	}
	auto &buffer = buffers[bufferId];
	buffer.clear();

	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			debug_log("bufferCopyRef: skipping buffer %d as it's the target\n\r", sourceId);
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			auto &sourceBuffer = sourceBufferIter->second;
			// push pointers to the blocks into our target buffer
			buffer.insert(buffer.end(), sourceBuffer.begin(), sourceBuffer.end());
		} else {
			debug_log("bufferCopyRef: buffer %d not found\n\r", sourceId);
		}
	}
	debug_log("bufferCopyRef: copied %d block references into buffer %d\n\r", buffers[bufferId].size(), bufferId);
}

// VDU 23, 0, &A0, bufferId; &1A, sourceBufferId; sourceBufferId; ...; 65535; : Copy blocks from buffers and consolidate
// Copy (blocks from) a list of buffers into a new buffer and consolidate them
// list is terminated with a bufferId of 65535 (-1)
// Replaces the target buffer with the new one, but will re-use the memory if it is the same size
// This is useful for constructing bitmaps from multiple buffers without needing an extra consolidate step
// If target buffer is included in the source list it will be skipped.
//
void VDUStreamProcessor::bufferCopyAndConsolidate(uint16_t bufferId, tcb::span<const uint16_t> sourceBufferIds) {
	if (bufferId == 65535) {
		debug_log("bufferCopyAndConsolidate: ignoring buffer %d\n\r", bufferId);
		return;
	}

	// work out total length of buffer
	uint32_t length = 0;
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			auto &sourceBuffer = sourceBufferIter->second;
			for (const auto &block : sourceBuffer) {
				length += block->size();
			}
		}
	}

	// Ensure the buffer has 1 block of the correct size
	auto &buffer = buffers[bufferId];
	if (buffer.size() != 1 || buffer.front()->size() != length)
	{
		buffer.clear();
		auto bufferStream = make_shared_psram<BufferStream>(length);
		if (!bufferStream || !bufferStream->getBuffer()) {
			// buffer couldn't be created
			debug_log("bufferCopyAndConsolidate: failed to create buffer %d\n\r", bufferId);
			return;
		}
		buffer.push_back(std::move(bufferStream));
	}

	auto destination = buffer.front()->getBuffer();

	// loop thru buffer IDs
	for (const auto sourceId : sourceBufferIds) {
		if (sourceId == bufferId) {
			debug_log("bufferCopyAndConsolidate: skipping buffer %d as it's the target\n\r", sourceId);
			continue;
		}
		auto sourceBufferIter = buffers.find(sourceId);
		if (sourceBufferIter != buffers.end()) {
			// buffer ID exists
			auto &sourceBuffer = sourceBufferIter->second;
			// loop thru blocks stored against this ID
			for (const auto &block : sourceBuffer) {
				// copy the block into our target buffer
				auto bufferLength = block->size();
				memcpy(destination, block->getBuffer(), bufferLength);
				destination += bufferLength;
			}
		} else {
			debug_log("bufferCopyAndConsolidate: buffer %d not found\n\r", sourceId);
		}
	}
	debug_log("bufferCopyAndConsolidate: copied %d bytes into buffer %d\n\r", length, bufferId);
}

#endif // VDU_BUFFERED_H
