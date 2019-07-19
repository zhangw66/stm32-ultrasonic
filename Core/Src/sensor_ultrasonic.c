#include <sensor_ultrasonic.h>
#include <ultra_hc_sr04.h>
#include <stdio.h>
//workaround
#define pr_err printf
#define get_ultra_type_str(id) ({ \
    char *name;                   \
    switch (id)                   \
    {                             \
    case ultra_type_hc_sr04:      \
        name = "hc_sr04";         \
        break;                    \
    default:                      \
        name = "unkown";          \
    }                             \
    name;                         \
})
struct ultrasonic ultrasonic_table[ultra_max_nums];
int8_t ultrasonic_install_hook(struct ultrasonic *ultra,
                               ultrasonic_init_t init,
                               ultrasonic_get_distance_t get_distance,
                               ultrasonic_get_ranging_freq_t get_ranging_freq)
{
    /*we must provide a get_distance callback*/
    if (!ultra || !get_distance) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    if (init)
        ultra->init = init;
    if (get_distance)
        ultra->get_distance = get_distance;
    if (get_ranging_freq)
        ultra->get_ranging_freq = get_ranging_freq;
    return 0;
}

int8_t ultrasonic_set_id(struct ultrasonic *ultra, ultra_id_t id, const char *name)
{
    int8_t ret = 0;
    if (!ultra) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    if (id >= 0 && id < ultra_max_nums)
        ultra->id = id;
    else {
        pr_err("%s[%d]: id<%d> is invalid!\n", __func__, __LINE__, id);
        return -1;
    }
    ultra->hardware_name = name ? name : NULL;
    return ret;
}

struct ultrasonic *ultrasonic_get_by_id(uint16_t id)
{
    struct ultrasonic *ultra = NULL;
    if (id < 0 || id >= ultra_max_nums) {
        pr_err("%s[%d]: ultra id<%u> is out of range\n", __func__, __LINE__, id);
        return NULL;
    }
    if ((ultrasonic_table[id].id == id)
        && ultrasonic_table[id].platform_data 
        && ultrasonic_table[id].get_distance) {
        //printf("%s[%d]: Get ultra-%d successful!!\n", __func__, __LINE__, id);
        ultra = &ultrasonic_table[id];
    } else
        pr_err("%s[%d]: ultra-%d is invalid,please setup first!!\n", __func__, __LINE__, id);
    return ultra;
}

int8_t ultrasonic_set_platdata(struct ultrasonic *ultra, void* data)
{
    int8_t ret = 0;
    if (!ultra || !data)
    {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    ultra->platform_data = data;
    return ret;
}

ultrasonic_ranging_state_machine_t ultra_get_ranging_state(struct ultrasonic *ultra)
{
    return ultra->ranging_state;
}

int8_t ultra_set_ranging_state(struct ultrasonic *ultra, ultrasonic_ranging_state_machine_t state)
{
    ultra->ranging_state = state;
    return 0; 
}

int8_t ultrasonic_config_state_machine(struct ultrasonic *ultra, set_ranging_state_t set_func, get_ranging_state_t get_func)
{
    int8_t ret = 0;
    if (!ultra) {
        pr_err("%s[%d]: args invalid!\n", __func__, __LINE__);
        return -1;
    }
    /*override the default function if needed*/
    ultra->set_ranging_state = (!set_func) ? ultra_set_ranging_state : set_func;
    ultra->get_ranging_state = (!get_func) ? ultra_get_ranging_state : get_func;
    return ret;
}

int8_t ultrasonic_setup(ultra_type_t type, uint16_t ultra_dev_nums)
{
    uint16_t ultra_nums;
    if (type < 0 || type >= ultra_type_nums) {
        pr_err("%s[%d]: ultra type<%u> is unknown\n", __func__, __LINE__, type);
        return -1;
    }
    //TODO:get how many ultrasonic we have in meissa board.
    ultra_nums = ultra_dev_nums;
    printf("%s:we have %d %s ultrasonic\n", __func__, ultra_nums, get_ultra_type_str(type));
    //Do corresponding initialization work according to different sensor types
    switch (type) {
    case ultra_type_hc_sr04:
        for (int i = 0; i < ultra_nums; i++) {
            ultrasonic_set_id(&ultrasonic_table[i], i, get_ultra_type_str(type));
            ultrasonic_config_state_machine(&ultrasonic_table[i], NULL, NULL);
            ultrasonic_install_hook(&ultrasonic_table[i], hc_sr04_init, hc_sr04_get_distance, hc_sr04_get_ranging_freq);
            ultrasonic_set_platdata(&ultrasonic_table[i], &hc_sr04_res[i]);
        }
        break;
    default:;
    }
    return 0;
}
