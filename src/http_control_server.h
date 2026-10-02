#ifndef HTTP_CONTROL_SERVER
#define HTTP_CONTROL_SERVER
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
//Called on a command being recieved from the HTTP Control Server
// cmd is the cmd received
// params is an array of strings interleaving the param names and their value, terminated by two NULL pointers
// the call back will set result_size to the size of the returned malloc'd data
// the call back will set mime_type to the desired mime type for the return; 
//Returns malloc'd data for a handled response or NULL for a non-handled response. 
typedef uint8_t* (*hcs_callback)(const char* cmd, const char** params, uint64_t* result_size, const char** mime_type);
//Update the HCS, and start/kill the server if needed
void hcs_update(bool enable, int64_t port, hcs_callback callback);

//Suspend and resume callbacks from multiple threads
void hcs_suspend_callbacks();
void hcs_resume_callbacks();

//Join this thread to the server thread
void hcs_join_server_thread();

// Live streams. A callback that returns the mime type "x-skyemu/stream-mjpeg" or
// "x-skyemu/stream-wav" turns the request into an endless MJPEG video or WAV sound stream of what
// the main thread publishes with these functions.
void hcs_stream_publish_frame(const uint8_t* jpeg, size_t size);
void hcs_stream_publish_audio(const int16_t* samples, size_t frames);  // 48 kHz stereo
int hcs_stream_video_clients();
int hcs_stream_audio_clients();
// IPv4 address of this device on the local network ("localhost" if unknown)
const char* hcs_local_ip();

#endif