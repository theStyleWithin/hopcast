/* HopCast - Phase 0 engine smoke test (original code, GPL-2.0-or-later).
 * Starts libobs without a display, loads plugins from the install prefix,
 * and checks that the core Linux input types registered. */
#include <obs.h>
#include <stdio.h>
#include <string.h>

static const char *required[] = {"ffmpeg_source", "v4l2_input", "pulse_input_capture", "image_source", NULL};

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <install-prefix>\n", argv[0]);
		return 2;
	}
	char bin[1024], data[1024];
	snprintf(bin, sizeof bin, "%s/lib/obs-plugins/%%module%%.so", argv[1]);
	snprintf(data, sizeof data, "%s/share/obs/obs-plugins/%%module%%", argv[1]);

	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "FAIL: obs_startup\n");
		return 1;
	}
	printf("libobs %s started\n", obs_get_version_string());

	obs_add_module_path(bin, data);
	obs_load_all_modules();
	obs_post_load_modules();

	int found[8] = {0}, total = 0;
	const char *id;
	for (size_t i = 0; obs_enum_input_types(i, &id); i++) {
		total++;
		for (int r = 0; required[r]; r++)
			if (strcmp(id, required[r]) == 0)
				found[r] = 1;
	}
	printf("input types registered: %d\n", total);

	int ok = 1;
	for (int r = 0; required[r]; r++) {
		printf("  %-22s %s\n", required[r], found[r] ? "ok" : "MISSING");
		ok &= found[r];
	}
	obs_shutdown();
	printf(ok ? "PASS\n" : "FAIL\n");
	return ok ? 0 : 1;
}
