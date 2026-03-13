#ifndef RADIAN_CONSTANTS_H
#define RADIAN_CONSTANTS_H

/* RADIAN protocol frame sizes (payload size in bytes) */
#define RADIAN_ACK_FRAME_SIZE   0x12   /* 18 bytes - meter ACK */
#define RADIAN_DATA_FRAME_SIZE 0x7C   /* 124 bytes - meter data with index */

/* Sync word bytes used in receive_radian_frame (CC1101 SYNC1/SYNC0) */
#define RADIAN_SYNC_PHASE1_H   0x55   /* 01010101 - start of sync pattern */
#define RADIAN_SYNC_PHASE1_L   0x50   /* 01010000 */
#define RADIAN_SYNC_PHASE2_H   0xFF   /* 11111111 - end of sync + start bit */
#define RADIAN_SYNC_PHASE2_L   0xF0   /* 11110000 */
#define RADIAN_SYNC_DEFAULT_H  0x55
#define RADIAN_SYNC_DEFAULT_L  0x00

/* parse_meter_report: byte offsets in decoded RADIAN report */
#define METER_REPORT_MIN_SIZE_LITERS  30
#define METER_REPORT_MIN_SIZE_EXTRA   48
#define METER_REPORT_OFFSET_LITERS_0  18
#define METER_REPORT_OFFSET_LITERS_1  19
#define METER_REPORT_OFFSET_LITERS_2  20
#define METER_REPORT_OFFSET_LITERS_3  21
#define METER_REPORT_OFFSET_BATTERY   31
#define METER_REPORT_OFFSET_TIME_START 44
#define METER_REPORT_OFFSET_TIME_END  45
#define METER_REPORT_OFFSET_READS_CTR 48

#endif /* RADIAN_CONSTANTS_H */
