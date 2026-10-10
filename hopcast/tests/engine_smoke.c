/* HopCast - Phase 0 engine smoke test (original code, GPL-2.0-or-later).
 *
 * Mirrors what a real frontend does on Linux:
 *   display -> obs_startup -> video (OpenGL) + audio -> load plugins
 * then checks that the core Linux input types registered and that a
 * source can be created and destroyed. Runs under Xvfb on headless CI. */
#include <obs.h>
#include <obs-nix-platform.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

static const char *required[] = {"ffmpeg_source", "v4l2_input", "pulse_input_capture", "image_source",
				 "color_source_v3", NULL};

#define FAIL(...)                              \
	do {                                   \
		fprintf(stderr, "FAIL: " __VA_ARGS__); \
		fprintf(stderr, "\n");          \
		rc = 1;                         \
		goto out;                       \
	} while (0)

int main(int argc, char **argv)
{
	int rc = 0;
	Display *dpy = NULL;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <install-prefix>\n", argv[0]);
		return 2;
	}
	char bin[1024], data[1024];
	snprintf(bin, sizeof bin, "%s/lib/obs-plugins/%%module%%.so", argv[1]);
	snprintf(data, sizeof data, "%s/share/obs/obs-plugins/%%module%%", argv[1]);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "FAIL: no X display (run under xvfb-run)\n");
		return 1;
	}
	obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
	obs_set_nix_platform_display(dpy);

	if (!obs_startup("en-US", NULL, NULL)) {
		XCloseDisplay(dpy);
		fprintf(stderr, "FAIL: obs_startup\n");
		return 1;
	}
	printf("libobs %s started\n", obs_get_version_string());

	struct obs_video_info ovi = {0};
	ovi.graphics_module = "libobs-opengl";
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = 1280;
	ovi.base_height = ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;
	int vr = obs_reset_video(&ovi);
	if (vr != OBS_VIDEO_SUCCESS)
		FAIL("obs_reset_video returned %d", vr);
	printf("video: OpenGL renderer up (%s)\n", gs_get_device_name());

	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai))
		FAIL("obs_reset_audio");
	printf("audio: 48 kHz stereo up\n");

	obs_add_module_path(bin, data);
	obs_load_all_modules();
	obs_log_loaded_modules();
	obs_post_load_modules();

	int found[16] = {0}, total = 0;
	const char *id;
	for (size_t i = 0; obs_enum_input_types(i, &id); i++) {
		total++;
		for (int r = 0; required[r]; r++)
			if (strcmp(id, required[r]) == 0)
				found[r] = 1;
	}
	printf("input types registered: %d\n", total);
	for (int r = 0; required[r]; r++) {
		printf("  %-22s %s\n", required[r], found[r] ? "ok" : "MISSING");
		if (!found[r])
			rc = 1;
	}

	/* A source can be created and rendered-owned objects torn down cleanly. */
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "width", 320);
	obs_data_set_int(s, "height", 180);
	obs_source_t *src = obs_source_create("color_source_v3", "smoke-color", s, NULL);
	obs_data_release(s);
	if (!src)
		FAIL("could not create color_source_v3");
	printf("created source '%s' (%ux%u)\n", obs_source_get_name(src), obs_source_get_width(src),
	       obs_source_get_height(src));
	obs_source_release(src);

out:
	obs_shutdown();
	XCloseDisplay(dpy);
	printf(rc == 0 ? "PASS\n" : "FAIL\n");
	return rc;
}
