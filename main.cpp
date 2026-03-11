#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <string>
#include <vector>

/* Build nkf as a single compilation unit (same as many upstream build recipes). */
#define PERL_XS 1
#include "utf8tbl.cpp"
#include "libnkf.hpp"

/* ---------------------------------------------------------------
 * In-memory string conversion
 * nkf_cnv を継承し、std_getc / std_ungetc / std_putc を
 * メモリバッファ版でオーバーライドする。
 * --------------------------------------------------------------- */
class nkf_cnv_str : public nkf_cnv
{
private:
	const unsigned char* m_src;
	size_t               m_src_len;
	size_t               m_src_pos;

	/* I/O コールバック */
	nkf_char my_getc(FILE*)
	{
		if (m_src_pos >= m_src_len)
			return EOF;
		return static_cast<nkf_char>(m_src[m_src_pos++]);
	}

	nkf_char my_ungetc(nkf_char c, FILE*)
	{
		if (c != EOF && m_src_pos > 0)
			--m_src_pos;
		return c;
	}

	void my_putc(nkf_char c)
	{
		if (c != EOF)
			m_dst.push_back(static_cast<unsigned char>(c));
	}

	/* I/O オーバーライド: module_connection() の直後に自動で呼ばれる */
	void wire_io_hook() override
	{
		i_getc    = static_cast<nkf_char(nkf_cnv::*)(FILE*)>          (&nkf_cnv_str::my_getc);
		i_ungetc  = static_cast<nkf_char(nkf_cnv::*)(nkf_char, FILE*)>(&nkf_cnv_str::my_ungetc);
		i_bgetc   = i_getc;
		i_bungetc = i_ungetc;
		o_putc    = static_cast<void(nkf_cnv::*)(nkf_char)>           (&nkf_cnv_str::my_putc);
		o_mputc   = o_putc;
	}

public:
	std::vector<unsigned char> m_dst;

	nkf_cnv_str(const char* src, size_t src_len)
		: nkf_cnv()
		, m_dst()
		, m_src(reinterpret_cast<const unsigned char*>(src))
		, m_src_len(src_len)
		, m_src_pos(0)
	{
		m_dst.reserve(src_len * 2);
	}
};

/*
 * nkf_convert_string()
 *   opts    : nkf オプション文字列
 *               -Sw  : 入力 Shift_JIS、出力 UTF-8
 *               -Ws  : 入力 UTF-8、  出力 Shift_JIS
 *   src     : 変換元バイト列
 *   src_len : バイト数
 *
 *   戻り値 : 変換後文字列 (std::string)
 *   例外   : std::runtime_error
 */
static std::string nkf_convert_string(const char* opts, const char* src, size_t src_len)
{
	nkf_cnv_str cnv(src, src_len);
	cnv.options(reinterpret_cast<unsigned char*>(const_cast<char*>(opts)));
	cnv.kanji_convert(nullptr);
	return std::string(cnv.m_dst.begin(), cnv.m_dst.end());
}

/* ---------------------------------------------------------------
 * ユーティリティ
 * --------------------------------------------------------------- */
static void die_usage(const char* exe)
{
	throw std::invalid_argument(
		std::string("Usage: ") + exe + " <nkf-options> <file>\n"
		"Example (Shift_JIS -> UTF-8): " + exe + " -Sw input.txt"
	);
}

int main(int argc, char** argv)
{
	try
	{
		if (argc < 3) {
			die_usage(argc > 0 ? argv[0] : "nkf");
		}

		/* --- 文字列変換往復テスト: SJIS -> UTF-8 -> SJIS --- */
		// "日本語" の Shift_JIS バイト列
		const unsigned char sjis_src[] =
			"「しばらく、しばらくお待ち下さい」兼高討九郎はそわそわしながら急に面をあげて云った、「ただいまお達しの御意、いまいちど仰せ聞けられとうございます」"
			"「その必要はない」老職水野主馬は、討九郎がそう云うだろうとかねて期していたようすで、あらぬ方へ眼をやりながら云った、「きたる六月より徒士かち組支配を免じ、馳走番仰せつけらる、それだけのことだ、わかったら退ってよろしい」"
			"「それは、その、御上意でございますか」"
			"「勿論もちろんのことだ」"
			"「もしや人違いではございませんか、兼高には与右衛門もおり、玄蕃もおります、わたくしに馳走番のお達しはちと解しかねまするが」"
			"「穏やかならぬぞ兼高」主馬は屹きっとふり向いた、「お上の御意を不服だと申すのか」"
			"「もったいない、決してさようなことはございません、決してさような」"
			"「では有難くお受けをするがよい」";
		// { 0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA };
		const size_t        sjis_len = sizeof(sjis_src);

		std::string utf8 = nkf_convert_string("-Sw",reinterpret_cast<const char*>(sjis_src), sjis_len);

		std::string sjis_back = nkf_convert_string("-Ws",utf8.c_str(), utf8.size());
		//printf((const char*)sjis_src);
		//printf((const char*)sjis_back.c_str());
		if (sjis_back.size() == sjis_len &&
			memcmp(sjis_back.data(), sjis_src, sjis_len) == 0)
		{
			fprintf(stdout, "Round-trip OK: SJIS -> UTF-8 -> SJIS matched.\n");
		}
		else
		{
			throw std::runtime_error("Round-trip FAILED: SJIS -> UTF-8 -> SJIS mismatch.");
		}
	}
	catch (const std::invalid_argument& e)
	{
		fprintf(stderr, "Usage error: %s\n", e.what());
		return 2;
	}
	catch (const std::runtime_error& e)
	{
		fprintf(stderr, "Error: %s\n", e.what());
		return 1;
	}
	return 0;
}
