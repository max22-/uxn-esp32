#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define WIDTH 512
#define HEIGHT 320

#define str(x) stra(x)
#define stra(x) #x

#define die(fnname) \
	do { \
		perror(fnname); \
		exit(EXIT_FAILURE); \
	} while(0)

#define x(fn, ...) \
	do { \
		if(fn(__VA_ARGS__) < 0) { \
			perror(#fn); \
			exit(EXIT_FAILURE); \
		} \
	} while(0)

int fix_fft(short *fr, short *fi, short m, short inverse);

static pid_t
launch_xvfb(void)
{
	char displayfd[16];
	int r;
	pid_t pid;
	int fds[2];
	x(pipe2, &fds[0], 0);
	pid = fork();
	if(pid < 0) {
		die("fork");
	} else if(!pid) {
		x(snprintf, displayfd, sizeof(displayfd), "%d", fds[1]);
		x(close, fds[0]);
		execlp("Xvfb", "Xvfb", "-screen", "0", str(WIDTH) "x" str(HEIGHT) "x24", "-fbdir", ".", "-displayfd", displayfd, "-nolisten", "tcp", NULL);
		die("execl");
		exit(EXIT_FAILURE);
	}
	x(close, fds[1]);
	r = read(fds[0], &displayfd[1], sizeof(displayfd) - 1);
	if(r < 0) die("read");
	x(close, fds[0]);
	displayfd[r] = '\0';
	displayfd[0] = ':';
	x(setenv, "DISPLAY", displayfd, 1);
	x(setenv, "ALSA_CONFIG_PATH", "asoundrc", 1);
	return pid;
}

static pid_t
launch_uxnemu(int *write_fd, int *read_fd, int *sound_fd)
{
	pid_t pid;
	int fds[6];
	x(pipe2, &fds[0], O_CLOEXEC);
	x(pipe2, &fds[2], O_CLOEXEC);
	x(pipe2, &fds[4], O_CLOEXEC);
	pid = fork();
	if(pid < 0) {
		die("fork");
	} else if(!pid) {
		x(dup2, fds[0], 0);
		x(dup2, fds[3], 1);
		x(dup2, fds[5], 3);
		execl("../../bin/uxnemu", "uxnemu", "autotest.rom", NULL);
		die("execl");
	}
	x(close, fds[0]);
	x(close, fds[3]);
	x(close, fds[5]);
	*write_fd = fds[1];
	*read_fd = fds[2];
	*sound_fd = fds[4];
	return pid;
}

static void
terminate(pid_t pid)
{
	int signals[] = {SIGINT, SIGTERM, SIGKILL};
	int status;
	size_t i;
	for(i = 0; i < sizeof(signals) / sizeof(int) * 10; ++i) {
		if(kill(pid, signals[i / 10])) {
			break;
		}
		usleep(100000);
		if(pid == waitpid(pid, &status, WNOHANG)) {
			return;
		}
	}
	waitpid(pid, &status, 0);
}

static int
open_framebuffer(void)
{
	for(;;) {
		int fd = open("Xvfb_screen0", O_RDONLY | O_CLOEXEC);
		if(fd >= 0) {
			return fd;
		}
		if(errno != ENOENT) {
			perror("open");
			return fd;
		}
		usleep(100000);
	}
}

#define PPM_HEADER "P6\n" str(WIDTH) " " str(HEIGHT) "\n255\n"

static void
save_screenshot(int fb_fd, const char *filename)
{
	unsigned char screen[WIDTH * HEIGHT * 4 + 4];
	int fd = open(filename, O_WRONLY | O_CREAT, 0666);
	int i;
	if(fd < 0) {
		die("screenshot open");
	}
	x(write, fd, PPM_HEADER, strlen(PPM_HEADER));
	x(lseek, fb_fd, 0xca0, SEEK_SET);
	x(read, fb_fd, &screen[4], WIDTH * HEIGHT * 4);
	for(i = 0; i < WIDTH * HEIGHT; ++i) {
		screen[i * 3 + 2] = screen[i * 4 + 4];
		screen[i * 3 + 1] = screen[i * 4 + 5];
		screen[i * 3 + 0] = screen[i * 4 + 6];
	}
	x(write, fd, screen, WIDTH * HEIGHT * 3);
	x(close, fd);
}

static void
systemf(char *format, ...)
{
	char *command;
	va_list ap;
	va_start(ap, format);
	x(vasprintf, &command, format, ap);
	system(command);
	free(command);
}

int uxn_read_fd, sound_fd;

static int
byte(void)
{
	char c;
	if(read(uxn_read_fd, &c, 1) != 1) {
		return 0;
	}
	return (unsigned char)c;
}

#define NEW_FFT_SIZE_POW2 10
#define NEW_FFT_SIZE (1 << NEW_FFT_SIZE_POW2)
#define NEW_FFT_USEC (5000 * NEW_FFT_SIZE / 441)

unsigned char left_peak, right_peak;

static int
detect_peak(short *real, short *imag)
{
	int i, peak = 0, peak_i;
	for(i = 0; i < NEW_FFT_SIZE; ++i) {
		int v = real[i] * real[i] + imag[i] * imag[i];
		if(peak < v) {
			peak = v;
			peak_i = i;
		} else if(peak > v * 10) {
			return peak_i;
		}
	}
	return 0;
}

static int
analyse_sound(short *samples)
{
	short real[NEW_FFT_SIZE], imag[NEW_FFT_SIZE];
	int i;
	for(i = 0; i < NEW_FFT_SIZE * 2; ++i) {
		if(samples[i * 2]) break;
	}
	if(i == NEW_FFT_SIZE * 2) return 0;
	for(i = 0; i < NEW_FFT_SIZE; ++i) {
		real[i] = samples[i * 4];
		imag[i] = samples[i * 4 + 2];
	}
	fix_fft(real, imag, NEW_FFT_SIZE_POW2, 0);
	return detect_peak(real, imag);
}

static int
read_sound(void)
{
	static short samples[NEW_FFT_SIZE * 4];
	static size_t len = 0;
	int r = read(sound_fd, ((char *)samples) + len, sizeof(samples) - len);
	if(r > 0) {
		len += r;
		if(len == sizeof(samples)) {
			left_peak = analyse_sound(&samples[0]);
			right_peak = analyse_sound(&samples[1]);
			len = 0;
			return 1;
		}
	}
	return 0;
}

static void
main_loop(int uxn_write_fd, int fb_fd)
{
	struct timeval next_sound = {0, 0};
	for(;;) {
		struct timeval now;
		struct timeval *timeout;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(uxn_read_fd, &fds);
		x(gettimeofday, &now, NULL);
		if(now.tv_sec > next_sound.tv_sec || (now.tv_sec == next_sound.tv_sec && now.tv_usec > next_sound.tv_usec)) {
			FD_SET(sound_fd, &fds);
			timeout = NULL;
		} else {
			now.tv_sec = 0;
			now.tv_usec = NEW_FFT_USEC;
			timeout = &now;
		}
		x(select, uxn_read_fd > sound_fd ? uxn_read_fd + 1 : sound_fd + 1, &fds, NULL, NULL, timeout);
		if(FD_ISSET(uxn_read_fd, &fds)) {
			int c, x, y;
			unsigned char blue;
			switch(c = byte()) {
			case 0x00: /* also used for EOF */
				printf("exiting\n");
				return;
			/* 01-06 mouse */
			case 0x01 ... 0x05:
				systemf("xdotool click %d", c);
				break;
			case 0x06:
				x = (byte() << 8) | byte();
				y = (byte() << 8) | byte();
				systemf("xdotool mousemove %d %d", x, y);
				break;
			/* 07-08 Screen */
			case 0x07:
				x = (byte() << 8) | byte();
				y = (byte() << 8) | byte();
				lseek(fb_fd, 0xca0 + (x + y * WIDTH) * 4, SEEK_SET);
				read(fb_fd, &blue, 1);
				blue = blue / 0x11;
				write(uxn_write_fd, &blue, 1);
				break;
			case 0x08:
				save_screenshot(fb_fd, "test.ppm");
				break;
			/* 09-0a Audio */
			case 0x09:
				write(uxn_write_fd, &left_peak, 1);
				break;
			case 0x0a:
				write(uxn_write_fd, &right_peak, 1);
				break;
			/* 11-7e Controller/key */
			case 0x11 ... 0x1c:
				systemf("xdotool key F%d", c - 0x10);
				break;
			case '0' ... '9':
			case 'A' ... 'Z':
			case 'a' ... 'z':
				systemf("xdotool key %c", c);
				break;
			default:
				printf("unhandled command 0x%02x\n", c);
				break;
			}
		}
		if(FD_ISSET(sound_fd, &fds)) {
			if(!next_sound.tv_sec) {
				x(gettimeofday, &next_sound, NULL);
			}
			next_sound.tv_usec += NEW_FFT_USEC * read_sound();
			if(next_sound.tv_usec > 1000000) {
				next_sound.tv_usec -= 1000000;
				++next_sound.tv_sec;
			}
		}
	}
}

int
main(void)
{
	pid_t xvfb_pid = launch_xvfb();
	int fb_fd = open_framebuffer();
	if(fb_fd >= 0) {
		int uxn_write_fd;
		pid_t uxnemu_pid = launch_uxnemu(&uxn_write_fd, &uxn_read_fd, &sound_fd);
		main_loop(uxn_write_fd, fb_fd);
		terminate(uxnemu_pid);
		x(close, uxn_write_fd);
		x(close, uxn_read_fd);
		x(close, sound_fd);
		x(close, fb_fd);
	}
	terminate(xvfb_pid);
	return 0;
}
