#pragma once

#include <stdint.h>

struct AgentContext
{
    uint8_t h0000[0x1AC];
    uint32_t timer;
};

struct CharContext
{
    uint8_t h0000[0x74];
    wchar_t playerName[20];
    uint8_t h009C[0x124];
    uint32_t mapid;
};

struct GameContext
{
    uint8_t h0000[8];
    AgentContext *agent;
    uint8_t h000C[0x38];
    CharContext *character;
};
