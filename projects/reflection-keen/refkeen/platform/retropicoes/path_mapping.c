/*
 * Port-side path mapping for the ReflectionHLE Keen Dreams backend on Pico.
 * See path_mapping.h for the contract and for the upstream evidence behind
 * every constant used here.
 *
 * No file system calls: this translation unit must stay host-testable.
 */

#include "path_mapping.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Bounds, and the two compile-time facts the gate also checks at run time.
 *
 * The static assertions are here so that a changed bound cannot silently stop
 * agreeing with the header: they are the compiler-side half of the same check
 * Test-PathMappingContract.ps1 performs against the upstream headers.
 * -------------------------------------------------------------------------- */

typedef char retropicoes_path_assert_dos_name_max_is_12[
	(RETROPICOES_PATH_DOS_NAME_MAX == 12) ? 1 : -1];
typedef char retropicoes_path_assert_medium_bound_is_256[
	(RETROPICOES_PATH_MEDIUM_BOUND == 256) ? 1 : -1];
typedef char retropicoes_path_assert_long_name_fits[
	(RETROPICOES_PATH_LONG_NAME_MAX + 1 < RETROPICOES_PATH_MEDIUM_BOUND) ? 1 : -1];

typedef struct {
	const char *base;
	bool writable;
	size_t name_max;
} retropicoes_path_role_info_t;

static const retropicoes_path_role_info_t g_retropicoes_path_roles[RETROPICOES_PATH_ROLE_COUNT] = {
	[RETROPICOES_PATH_ROLE_GAME_DATA] = {
		RETROPICOES_PATH_GAME_DATA_BASE,
		false,
		RETROPICOES_PATH_DOS_NAME_MAX
	},
	[RETROPICOES_PATH_ROLE_SAVE_ROOT] = {
		RETROPICOES_PATH_SAVE_ROOT_BASE,
		true,
		RETROPICOES_PATH_LONG_NAME_MAX
	}
};

static bool retropicoes_path_role_valid(retropicoes_path_role_t role)
{
        return (int)role < (int)RETROPICOES_PATH_ROLE_COUNT;
}

const char *retropicoes_path_role_base(retropicoes_path_role_t role)
{
	if (!retropicoes_path_role_valid(role))
		return NULL;
	return g_retropicoes_path_roles[role].base;
}

bool retropicoes_path_role_is_writable(retropicoes_path_role_t role)
{
	if (!retropicoes_path_role_valid(role))
		return false;
	return g_retropicoes_path_roles[role].writable;
}

size_t retropicoes_path_role_name_max(retropicoes_path_role_t role)
{
	if (!retropicoes_path_role_valid(role))
		return 0;
	return g_retropicoes_path_roles[role].name_max;
}

/* --------------------------------------------------------------------------
 * Name validation
 * -------------------------------------------------------------------------- */

static bool retropicoes_path_byte_is_illegal(unsigned char c)
{
	/* Path-significant or illegal on the target media. Note that '.' is *not*
	   in this set: dots are legal inside a name, they are only rejected when
	   the name is exactly "." or "..". */
	switch (c)
	{
	case '/':
	case '\\':
	case ':':
	case '"':
	case '*':
	case '?':
	case '<':
	case '>':
	case '|':
		return true;
	default:
		return false;
	}
}

retropicoes_path_result_t retropicoes_path_check_name(const char *name, size_t name_max)
{
	size_t len;

	if ((!name) || (!*name))
		return RETROPICOES_PATH_ERR_NAME_EMPTY;

	if (name[0] == '.' &&
	    ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0'))))
		return RETROPICOES_PATH_ERR_NAME_DOT;

	for (len = 0; name[len]; ++len)
	{
		unsigned char c = (unsigned char)name[len];

		if ((c < 32) || (c > 126))
			return RETROPICOES_PATH_ERR_NAME_NON_ASCII;
		if (retropicoes_path_byte_is_illegal(c))
			return RETROPICOES_PATH_ERR_NAME_ILLEGAL;
	}

	if (name[len - 1] == ' ')
		return RETROPICOES_PATH_ERR_NAME_TRAILING_SPACE;

	/* `len` excludes the NUL, and so does `name_max`. */
	if (len > name_max)
		return RETROPICOES_PATH_ERR_NAME_TOO_LONG;

	return RETROPICOES_PATH_OK;
}

/* --------------------------------------------------------------------------
 * Canonicalisation and composition
 * -------------------------------------------------------------------------- */

static char retropicoes_path_tolower(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ? (char)(c - 'A' + 'a') : c;
}

retropicoes_path_result_t retropicoes_path_canonical_name(
	const char *name, char *out, size_t out_size)
{
	size_t len;
	retropicoes_path_result_t result;

	/* Canonicalisation never makes a name longer, so the caller's own bound
	   decides validity; RETROPICOES_PATH_LONG_NAME_MAX is the loosest rule
	   this module knows about. Callers with a tighter rule must call
	   retropicoes_path_check_name first (retropicoes_path_for_role does). */
	result = retropicoes_path_check_name(name, RETROPICOES_PATH_LONG_NAME_MAX);
	if (result != RETROPICOES_PATH_OK)
		return result;

	len = strlen(name);
	if ((!out) || (out_size < (len + 1)))
		return RETROPICOES_PATH_ERR_TRUNCATED;

	for (size_t i = 0; i < len; ++i)
		out[i] = retropicoes_path_tolower(name[i]);
	out[len] = '\0';

	return RETROPICOES_PATH_OK;
}

retropicoes_path_result_t retropicoes_path_compose(
	const char *base, const char *name, size_t name_max, char *out, size_t out_size)
{
	size_t base_len;
	size_t name_len;
	size_t total;
	retropicoes_path_result_t result;

	if ((!base) || (!*base))
		return RETROPICOES_PATH_ERR_ROLE;

	/* Reject a base that would itself break the medium bound: an over-long base
	   is a build-time programming error, not a name problem. */
	base_len = strlen(base);
	if ((base_len + 1) > RETROPICOES_PATH_MEDIUM_BOUND)
		return RETROPICOES_PATH_ERR_TRUNCATED;

	result = retropicoes_path_check_name(name, name_max);
	if (result != RETROPICOES_PATH_OK)
		return result;

	name_len = strlen(name);
	/* base + '/' + name + NUL */
	total = base_len + 1 + name_len + 1;
	if (total > RETROPICOES_PATH_MEDIUM_BOUND)
		return RETROPICOES_PATH_ERR_NAME_TOO_LONG;
	if ((!out) || (out_size < total))
		return RETROPICOES_PATH_ERR_TRUNCATED;

	memcpy(out, base, base_len);
	out[base_len] = '/';
	for (size_t i = 0; i < name_len; ++i)
		out[base_len + 1 + i] = retropicoes_path_tolower(name[i]);
	out[total - 1] = '\0';

	return RETROPICOES_PATH_OK;
}

retropicoes_path_result_t retropicoes_path_for_role(
	retropicoes_path_role_t role, const char *name, char *out, size_t out_size)
{
	if (!retropicoes_path_role_valid(role))
		return RETROPICOES_PATH_ERR_ROLE;

	return retropicoes_path_compose(
		g_retropicoes_path_roles[role].base,
		name,
		g_retropicoes_path_roles[role].name_max,
		out,
		out_size);
}

retropicoes_path_result_t retropicoes_path_writable_dir(
	const char *version_dir, char *out, size_t out_size)
{
	return retropicoes_path_compose(
		RETROPICOES_PATH_SAVE_ROOT_BASE,
		version_dir,
		RETROPICOES_PATH_LONG_NAME_MAX,
		out,
		out_size);
}

/* --------------------------------------------------------------------------
 * Diagnostics
 * -------------------------------------------------------------------------- */

const char *retropicoes_path_result_str(retropicoes_path_result_t result)
{
	switch (result)
	{
	case RETROPICOES_PATH_OK:
		return "ok";
	case RETROPICOES_PATH_ERR_ROLE:
		return "unknown path role";
	case RETROPICOES_PATH_ERR_NAME_EMPTY:
		return "empty name";
	case RETROPICOES_PATH_ERR_NAME_NON_ASCII:
		return "name has a non-ASCII byte";
	case RETROPICOES_PATH_ERR_NAME_ILLEGAL:
		return "name has a path-significant or illegal byte";
	case RETROPICOES_PATH_ERR_NAME_DOT:
		return "name is \".\" or \"..\"";
	case RETROPICOES_PATH_ERR_NAME_TRAILING_SPACE:
		return "name ends with a space";
	case RETROPICOES_PATH_ERR_NAME_TOO_LONG:
		return "name is too long";
	case RETROPICOES_PATH_ERR_TRUNCATED:
		return "output buffer too small";
	default:
		return "unrecognised result";
	}
}
