#pragma once
#define Displayf(fmt, ...) printf("[DEBUG] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)