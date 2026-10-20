"""Make a replay-only copy omitting explicit RTE declarations for RenderDoc.

This changes the floating-point contract and must never be applied to the game
as a purported fix. The caller must compare replay output before interpreting
any debugger trace. All other SPIR-V words, IDs and instructions are retained.
"""
import struct


def without_explicit_rte(data):
    if len(data) % 4:
        raise ValueError('SPIR-V byte length must be divisible by four')
    words = list(struct.unpack('<{}I'.format(len(data)//4), data))
    if len(words) < 5 or words[0] != 0x07230203:
        raise ValueError('Invalid SPIR-V header')
    result = words[:5]
    removed_capability = removed_mode = 0
    index = 5
    while index < len(words):
        count, opcode = words[index] >> 16, words[index] & 0xffff
        if not count or index + count > len(words):
            raise ValueError('Malformed SPIR-V instruction')
        instruction = words[index:index+count]
        if opcode == 17 and instruction[1:] == [4467]:
            removed_capability += 1  # OpCapability RoundingModeRTE
        elif opcode == 16 and count == 4 and instruction[2:] == [4462, 32]:
            removed_mode += 1  # OpExecutionMode <entry> RoundingModeRTE 32
        else:
            result.extend(instruction)
        index += count
    if (removed_capability, removed_mode) != (1, 1):
        raise ValueError('Expected exactly one float32 RTE capability and execution mode')
    return struct.pack('<{}I'.format(len(result)), *result)
