/*
 * Host test for the port path mapping contract (refkeen/platform/retropicoes/
 * path_mapping.h).
 *
 * This test owns the *declared* vectors. Test-PathMappingContract.ps1 does not
 * trust them: it re-derives the same names from the upstream headers
 * (be_gamedefs_kdreams.h, be_gamestr.h) and fails if the two ever disagree.
 * That split is deliberate -- a vector copied by hand into a test is not
 * evidence, it is a second guess.
 *
 * Build:
 *   gcc -std=c99 -Wall -Wextra -Werror -I refkeen/platform/retropicoes \
 *       -o test_path_mapping refkeen/tests/test_path_mapping.c \
 *       refkeen/platform/retropicoes/path_mapping.c
 */

#include "path_mapping.h"

#include <stdio.h>
#include <string.h>

static unsigned g_checks;
static unsigned g_failures;

static void check(bool condition, const char *what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		printf("FAIL  %s\n", what);
	}
}

static void check_str(const char *got, const char *want, const char *what)
{
	char message[512];
	++g_checks;
	if (!got || strcmp(got, want))
	{
		++g_failures;
		snprintf(message, sizeof(message), "%s: got \"%s\", want \"%s\"",
		         what, got ? got : "(null)", want);
		printf("FAIL  %s\n", message);
	}
}

static void check_result(
	retropicoes_path_result_t got, retropicoes_path_result_t want, const char *what)
{
	char message[512];
	++g_checks;
	if (got != want)
	{
		++g_failures;
		snprintf(message, sizeof(message), "%s: got %s, want %s",
		         what, retropicoes_path_result_str(got),
		         retropicoes_path_result_str(want));
		printf("FAIL  %s\n", message);
	}
}

/* ---------------------------------------------------------------------------
 * Vectors. Every name here is transcribed from the game definitions upstream
 * ships; the gate re-derives them.
 * ------------------------------------------------------------------------ */

static const char *const g_keen_required_files[] = {
	"AUDIO.KDR", "EGAGRAPH.KDR", "GAMEMAPS.KDR", "KDREAMS.EXE",   /* EGA v1.00 */
	"CGAGRAPH.KDR",                                                /* CGA v1.00 */
	"KDREAMS.AUD", "KDREAMS.CMP", "KDREAMS.EGA", "KDREAMS.MAP",    /* v1.13 */
	"LAST.SHL",                                                    /* v1.13 */
	"CONTEXT.KDR", "EGADICT.CGA", "EGADICT.KDR", "EGAHEAD.CGA",
	"EGAHEAD.KDR", "GAMETEXT.KDR", "KDREAMS.CGA", "KDREAMS.SND",
	"MAPDICT.KDR", "MAPHEAD.KDR", "SOUNDDCT.KDR", "SOUNDHHD.KDR",
	"STORY.KDR"                                                    /* 2015 */
};
#define KEEN_REQUIRED_FILES_COUNT \
	(sizeof(g_keen_required_files) / sizeof(g_keen_required_files[0]))

/* Everything the engine is known to write or read under the save root. */
static const struct {
	const char *name;
	const char *expect_suffix;
} g_save_root_names[] = {
	{ "SAVEGM0.KDR", "savegm0.kdr" },
	{ "SAVEGM6.KDR", "savegm6.kdr" },
	{ "KDREAMS.CFG", "kdreams.cfg" },
	{ "CONFIG.KDR", "config.kdr" },
	{ "reflectionhle.cfg", "reflectionhle.cfg" },
	{ "gamecontrollerdb.txt", "gamecontrollerdb.txt" }
};
#define SAVE_ROOT_NAMES_COUNT \
	(sizeof(g_save_root_names) / sizeof(g_save_root_names[0]))

/* writableFilesDir values, i.e. BE_STR_GAMEVER_KDREAMS* from be_gamestr.h. */
static const char *const g_keen_version_dirs[] = {
	"kdreamse100", "kdreamsc100", "kdreamse113", "kdreamsc105",
	"kdreamse193", "kdreamse120", "kdreams2015"
};
#define KEEN_VERSION_DIRS_COUNT \
	(sizeof(g_keen_version_dirs) / sizeof(g_keen_version_dirs[0]))

/* ---------------------------------------------------------------------------
 * Machine-readable contract summary
 *
 * The gate parses these lines instead of re-deriving the port's numbers from
 * source. That way the gate reads what the compiler actually produced, so a
 * macro that is defined but shadowed, or an expression the gate would have had
 * to evaluate by hand, cannot make the gate and the code disagree.
 * ------------------------------------------------------------------------ */

static void print_contract(void)
{
	printf("CONTRACT medium_bound=%d\n", (int)RETROPICOES_PATH_MEDIUM_BOUND);
	printf("CONTRACT dos_name_bound=%d\n", (int)RETROPICOES_PATH_DOS_NAME_BOUND);
	printf("CONTRACT dos_name_max=%d\n", (int)RETROPICOES_PATH_DOS_NAME_MAX);
	printf("CONTRACT long_name_max=%d\n", (int)RETROPICOES_PATH_LONG_NAME_MAX);
	printf("CONTRACT game_data_base=%s\n", RETROPICOES_PATH_GAME_DATA_BASE);
	printf("CONTRACT save_root_base=%s\n", RETROPICOES_PATH_SAVE_ROOT_BASE);

	for (int role = 0; role < (int)RETROPICOES_PATH_ROLE_COUNT; ++role)
	{
		const char *base = retropicoes_path_role_base((retropicoes_path_role_t)role);
		printf("CONTRACT role=%d base=%s name_max=%d writable=%d\n",
		       role,
		       base ? base : "(null)",
		       (int)retropicoes_path_role_name_max((retropicoes_path_role_t)role),
		       retropicoes_path_role_is_writable((retropicoes_path_role_t)role) ? 1 : 0);
	}

	for (size_t i = 0; i < KEEN_VERSION_DIRS_COUNT; ++i)
		printf("CONTRACT version_dir=%s\n", g_keen_version_dirs[i]);

	for (size_t i = 0; i < KEEN_REQUIRED_FILES_COUNT; ++i)
		printf("CONTRACT game_file=%s\n", g_keen_required_files[i]);

	for (size_t i = 0; i < SAVE_ROOT_NAMES_COUNT; ++i)
		printf("CONTRACT save_name=%s\n", g_save_root_names[i].name);
}


/* ---------------------------------------------------------------------------
 * Positive paths
 * ------------------------------------------------------------------------ */

static void test_role_table(void)
{
	check_str(retropicoes_path_role_base(RETROPICOES_PATH_ROLE_GAME_DATA),
	          "/data/refkeen", "game data base");
	check_str(retropicoes_path_role_base(RETROPICOES_PATH_ROLE_SAVE_ROOT),
	          "/SAVES/refkeen", "save root base");

	check(retropicoes_path_role_is_writable(RETROPICOES_PATH_ROLE_SAVE_ROOT),
	      "save root is writable");
	check(!retropicoes_path_role_is_writable(RETROPICOES_PATH_ROLE_GAME_DATA),
	      "game data is not writable");

	check(retropicoes_path_role_name_max(RETROPICOES_PATH_ROLE_GAME_DATA) ==
	      RETROPICOES_PATH_DOS_NAME_MAX,
	      "game data uses the DOS name bound");
	check(retropicoes_path_role_name_max(RETROPICOES_PATH_ROLE_SAVE_ROOT) ==
	      RETROPICOES_PATH_LONG_NAME_MAX,
	      "save root uses the long name bound");

	check(retropicoes_path_role_base(RETROPICOES_PATH_ROLE_COUNT) == NULL,
	      "out-of-range role has no base");
	check(retropicoes_path_role_name_max(RETROPICOES_PATH_ROLE_COUNT) == 0,
	      "out-of-range role has no name bound");
	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_COUNT, "A.KDR",
	              NULL, 0), RETROPICOES_PATH_ERR_ROLE, "out-of-range role rejected");
}

static void test_game_data_names(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];

	for (size_t i = 0; i < KEEN_REQUIRED_FILES_COUNT; ++i)
	{
		char want[RETROPICOES_PATH_MEDIUM_BOUND];
		size_t len = strlen(g_keen_required_files[i]);

		snprintf(want, sizeof(want), "/data/refkeen/%s",
		         g_keen_required_files[i]);
		for (size_t j = 0; want[j]; ++j)
			if ((want[j] >= 'A') && (want[j] <= 'Z'))
				want[j] = (char)(want[j] - 'A' + 'a');

		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_GAME_DATA,
		             g_keen_required_files[i], out, sizeof(out)),
		             RETROPICOES_PATH_OK, g_keen_required_files[i]);
		check_str(out, want, g_keen_required_files[i]);

		/* 11.3 at worst: every required file name must fit the DOS bound. */
		check(len <= RETROPICOES_PATH_DOS_NAME_MAX,
		      "required file name fits the DOS bound");
	}
}

static void test_save_root_names(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];

	for (size_t i = 0; i < SAVE_ROOT_NAMES_COUNT; ++i)
	{
		char want[RETROPICOES_PATH_MEDIUM_BOUND];

		snprintf(want, sizeof(want), "/SAVES/refkeen/%s",
		         g_save_root_names[i].expect_suffix);

		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
		             g_save_root_names[i].name, out, sizeof(out)),
		             RETROPICOES_PATH_OK, g_save_root_names[i].name);
		check_str(out, want, g_save_root_names[i].name);
	}
}

static void test_writable_dirs(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];

	for (size_t i = 0; i < KEEN_VERSION_DIRS_COUNT; ++i)
	{
		char want[RETROPICOES_PATH_MEDIUM_BOUND];

		snprintf(want, sizeof(want), "/SAVES/refkeen/%s", g_keen_version_dirs[i]);

		check_result(retropicoes_path_writable_dir(g_keen_version_dirs[i], out,
		             sizeof(out)), RETROPICOES_PATH_OK, g_keen_version_dirs[i]);
		check_str(out, want, g_keen_version_dirs[i]);

		/* This is exactly what upstream composes as writableFilesPath, so it
		   must never equal the save root itself (the port would then have to
		   create a directory it never names). */
		check(strcmp(out, RETROPICOES_PATH_SAVE_ROOT_BASE) != 0,
		      "writable dir is not the save root");
	}
}

static void test_canonicalisation(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];

	check_result(retropicoes_path_canonical_name("SAVEGM3.KDR", out, sizeof(out)),
	             RETROPICOES_PATH_OK, "canonicalise mixed case");
	check_str(out, "savegm3.kdr", "canonical form is lower case");

	check_result(retropicoes_path_canonical_name("already.kdr", out, sizeof(out)),
	             RETROPICOES_PATH_OK, "canonicalise already-lower");
	check_str(out, "already.kdr", "already-lower name unchanged");

	/* Digits and punctuation in the allowed set must survive untouched. */
	check_result(retropicoes_path_canonical_name("A1-B_2.KDR", out, sizeof(out)),
	             RETROPICOES_PATH_OK, "canonicalise with '-', '_', digits");
	check_str(out, "a1-b_2.kdr", "only A-Z is folded");
}

/* ---------------------------------------------------------------------------
 * Boundary cases -- exact fit, one short, one over
 * ------------------------------------------------------------------------ */

static void test_boundaries(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];
	char name[RETROPICOES_PATH_MEDIUM_BOUND];

	/* DOS bound: 12 characters excluding the NUL. */
	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_GAME_DATA,
	             "ABCDEFGH.KDR", out, sizeof(out)), RETROPICOES_PATH_OK,
	             "exact DOS-bound name accepted");

	memset(name, 'A', 13);
	name[13] = '\0';
	/* Assert the fixture itself before asserting the outcome. A boundary test
	   whose fixture silently has the wrong length proves nothing -- that is
	   exactly how an earlier version of this test passed a 13-char case that
	   was really 12. */
	check(strlen(name) == 13, "DOS fixture really is 13 characters");
	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_GAME_DATA,
	             name, out, sizeof(out)), RETROPICOES_PATH_ERR_NAME_TOO_LONG,
	             "DOS-bound + 1 rejected");

	/* Save root: 63 characters excluding the NUL. Both lengths are named
	   constants so the two cases can never collapse onto one value. */
	{
		const size_t max_len = RETROPICOES_PATH_LONG_NAME_MAX;
		const size_t over_len = max_len + 1;

		check(over_len > max_len, "long-bound cases are distinct");

		memset(name, 'a', max_len);
		name[max_len] = '\0';
		check(strlen(name) == max_len, "long fixture really is max_len");
		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
		             name, out, sizeof(out)), RETROPICOES_PATH_OK,
		             "exact long-bound name accepted");

		memset(name, 'a', over_len);
		name[over_len] = '\0';
		check(strlen(name) == over_len, "long fixture really is max_len + 1");
		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
		             name, out, sizeof(out)), RETROPICOES_PATH_ERR_NAME_TOO_LONG,
		             "long-bound + 1 rejected");
	}

	/* Output buffer: "/SAVES/refkeen/x.kdr" is 20 characters plus NUL, so a
	   20-byte buffer must fail and a 21-byte buffer must succeed. */
	memset(out, 'Z', sizeof(out));
	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
	             "x.kdr", out, 20), RETROPICOES_PATH_ERR_TRUNCATED,
	             "output buffer one byte short rejected");
	/* The truncation path must not have written anything: a half-built path
	   that happens to be NUL-terminated is far worse than no path at all. */
	check(out[0] == 'Z', "nothing written on the truncation path");

	memset(out, 'Z', sizeof(out));
	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
	             "x.kdr", out, 21), RETROPICOES_PATH_OK,
	             "output buffer exactly big enough accepted");
	check_str(out, "/SAVES/refkeen/x.kdr", "exact-fit composition");

	check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
	             "x.kdr", NULL, 21), RETROPICOES_PATH_ERR_TRUNCATED,
	             "NULL output rejected");

	/* An over-long base is a programming error, not a name error. */
	{
		char base[RETROPICOES_PATH_MEDIUM_BOUND + 8];
		memset(base, 'b', sizeof(base) - 1);
		base[sizeof(base) - 1] = '\0';
		check_result(retropicoes_path_compose(base, "x.kdr",
		             RETROPICOES_PATH_LONG_NAME_MAX, out, sizeof(out)),
		             RETROPICOES_PATH_ERR_TRUNCATED, "over-long base rejected");
	}
	check_result(retropicoes_path_compose("", "x.kdr",
	             RETROPICOES_PATH_LONG_NAME_MAX, out, sizeof(out)),
	             RETROPICOES_PATH_ERR_ROLE, "empty base rejected");
	check_result(retropicoes_path_compose(NULL, "x.kdr",
	             RETROPICOES_PATH_LONG_NAME_MAX, out, sizeof(out)),
	             RETROPICOES_PATH_ERR_ROLE, "NULL base rejected");
}

/* ---------------------------------------------------------------------------
 * Negative paths
 * ------------------------------------------------------------------------ */

static void test_rejections(void)
{
	char out[RETROPICOES_PATH_MEDIUM_BOUND];
	struct {
		const char *name;
		retropicoes_path_result_t want;
	} cases[] = {
		{ NULL,                      RETROPICOES_PATH_ERR_NAME_EMPTY },
		{ "",                        RETROPICOES_PATH_ERR_NAME_EMPTY },
		{ ".",                       RETROPICOES_PATH_ERR_NAME_DOT },
		{ "..",                      RETROPICOES_PATH_ERR_NAME_DOT },
		{ "../SAVEGM0.KDR",          RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "sub/SAVEGM0.KDR",         RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "sub\\SAVEGM0.KDR",        RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "C:SAVEGM0.KDR",           RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "A*B.KDR",                 RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "A?B.KDR",                 RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "A\"B.KDR",                RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "A<B>.KDR",                RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "A|B.KDR",                 RETROPICOES_PATH_ERR_NAME_ILLEGAL },
		{ "SAVEGM0.KDR ",            RETROPICOES_PATH_ERR_NAME_TRAILING_SPACE },
		{ "\tSAVEGM0.KDR",           RETROPICOES_PATH_ERR_NAME_NON_ASCII },
		{ "SAVEGM0.KDR\xff",         RETROPICOES_PATH_ERR_NAME_NON_ASCII },
		{ "SAVEGM0.KDR\x81",         RETROPICOES_PATH_ERR_NAME_NON_ASCII },
		{ "caf\xe9.kdr",             RETROPICOES_PATH_ERR_NAME_NON_ASCII },
		{ "SAVEGM0.KDR\n",           RETROPICOES_PATH_ERR_NAME_NON_ASCII }
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
	{
		/* Both roles must reject, since neither rule is role-specific. */
		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_SAVE_ROOT,
		             cases[i].name, out, sizeof(out)), cases[i].want,
		             cases[i].name ? cases[i].name : "(null)");
		check_result(retropicoes_path_for_role(RETROPICOES_PATH_ROLE_GAME_DATA,
		             cases[i].name, out, sizeof(out)), cases[i].want,
		             cases[i].name ? cases[i].name : "(null)");
	}

	check_result(retropicoes_path_writable_dir("../kdreamse100", out, sizeof(out)),
	             RETROPICOES_PATH_ERR_NAME_ILLEGAL, "version dir cannot escape");
	check_result(retropicoes_path_writable_dir("", out, sizeof(out)),
	             RETROPICOES_PATH_ERR_NAME_EMPTY, "empty version dir rejected");
}

/* ---------------------------------------------------------------------------
 * Diagnostics
 * ------------------------------------------------------------------------ */

static void test_result_strings(void)
{
	/* A missing case in the switch would silently return "unrecognised", which
	   is the one string callers must never see for a real code. */
	for (int i = 0; i <= (int)RETROPICOES_PATH_ERR_TRUNCATED; ++i)
	{
		const char *s = retropicoes_path_result_str(
			(retropicoes_path_result_t)i);
		check(s != NULL, "result string is never NULL");
		check(strcmp(s, "unrecognised result") != 0, "every code has its own text");
	}
	check(strcmp(retropicoes_path_result_str(RETROPICOES_PATH_OK), "ok") == 0,
	      "ok text");
	check(strcmp(retropicoes_path_result_str(
	      (retropicoes_path_result_t)999), "unrecognised result") == 0,
	      "unknown code text");
}

int main(void)
{
	test_role_table();
	test_game_data_names();
	test_save_root_names();
	test_writable_dirs();
	test_canonicalisation();
	test_boundaries();
	test_rejections();
	test_result_strings();

	print_contract();

	printf("%u check(s), %u failure(s)\n", g_checks, g_failures);
	if (g_failures)
	{
		printf("REFKEEN PATH MAPPING: FAIL\n");
		return 1;
	}
	printf("REFKEEN PATH MAPPING: PASS\n");
	return 0;
}
