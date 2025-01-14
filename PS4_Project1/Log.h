#pragma once
#define Displayf(fmt, ...) printf("[DEBUG] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#define Warningf(fmt, ...) printf("[WARNING] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)
#define Errorf(fmt, ...) printf("[ERROR] %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)