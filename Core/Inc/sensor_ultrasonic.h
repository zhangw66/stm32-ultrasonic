#ifndef __SENSOR_ULTRASONIC__
#define __SENSOR_ULTRASONIC__
#include <main.h>
#include <stdbool.h>
typedef enum ultra_type {
    ultra_type_hc_sr04,
    ultra_type_nums,
} ultra_type_t;
typedef enum {
    ultra_id_1,
    ultra_id_2,
    ultra_id_3,
    ultra_max_nums,
} ultra_id_t;
typedef enum ultrasonic_ranging_state_machine {
    HC_SR04_STATE_BEGIN,
    HC_SR04_STATE_IDLE,
    HC_SR04_STATE_SEND_ULTRASOUND,
    HC_SR04_STATE_RAISE_ECHO_START,
    HC_SR04_STATE_RAISE_ECHO_END,
    HC_SR04_STATE_RAISE_CALC_DIS,
    HC_SR04_STATE_END,
    ULTRA_STATE_UNKNOWN,
} ultrasonic_ranging_state_machine_t;
typedef struct ultrasonic {
    ultra_id_t id;
    enum ultra_type type;
    uint32_t timeout;
    uint32_t echo_pulse; //unit:us
    volatile ultrasonic_ranging_state_machine_t ranging_state;
    const char *hardware_name;
    uint16_t max_ranging_freq;	//unit:HZ
    void *platform_data;
    int8_t (*init)(struct ultrasonic *);
    int16_t (*get_distance)(struct ultrasonic *);
    uint16_t (*get_ranging_freq)(struct ultrasonic *);
    ultrasonic_ranging_state_machine_t (*get_ranging_state)(struct ultrasonic *);
    int8_t (*set_ranging_state)(struct ultrasonic *, ultrasonic_ranging_state_machine_t);
} ultrasonic_t;

typedef int8_t (*ultrasonic_init_t)(struct ultrasonic *);
typedef int16_t (*ultrasonic_get_distance_t)(struct ultrasonic *);
typedef uint16_t (*ultrasonic_get_ranging_freq_t)(struct ultrasonic *);
typedef ultrasonic_ranging_state_machine_t (*get_ranging_state_t)(struct ultrasonic *);
typedef int8_t (*set_ranging_state_t)(struct ultrasonic *, ultrasonic_ranging_state_machine_t);
extern int8_t ultrasonic_setup(ultra_type_t type, uint16_t ultra_dev_nums);
extern struct ultrasonic *ultrasonic_get_by_id(uint16_t id);
#endif
