## ffmpeg4 capture camera to rtmp

Recently, due to the need of work plan, research on ffmpeg was started.

For the command line push service, it is actually very simple. But it is not easy to control when it is integrated in its own program. Especially when the parent process shuts down abnormally, the ffmpeg process may not be killed, causing some exceptions. In addition, the biggest problem with the command line is that it cannot manage the ffmpeg push stream in a fine-grained manner.

ffmpeg bought some courses online and found that most of them are for ffmpeg3, the current ffmpeg is mainly 4. The syntax is quite different.

After a week of research, I found that a lot of code on the Internet is somewhat problematic.

Finally, I used ffmpeg4 to implement a version of the camera to capture the stream. Some pits are marked here. Some of the difficulties and tricks are not explained in many places, I will record them here.

