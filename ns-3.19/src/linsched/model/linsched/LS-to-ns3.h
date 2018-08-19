#ifdef __cplusplus
extern "C" {
#endif

void schedule_ns3_event(unsigned long time, void (*f)(void *data), void *data);
unsigned long get_ns3_time(void);

#ifdef __cplusplus
}
#endif
