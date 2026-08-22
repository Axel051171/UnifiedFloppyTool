#include "stdafx.h"

class ScriptEngine {
public:
	ScriptEngine(RawDisk& raw_disk);

	void EmitByte(bool special, uint8_t c);
	void EmitPadBits(uint32_t count, bool set);
	void EmitCellDelay(uint32_t count256);
	void EmitCellDelayNoFlux(uint32_t count256);

	void BeginCRC();
	void EndCRC();

	void BeginTrack(int track, int side);
	void EndTrack();

	void SetGeometry(int tracks, int sides);

private:
	RawDisk& mRawDisk;
	RawTrack *mpCurrentTrack = nullptr;
	int mCurrentLogicalTrackNum = 0;
	uint32_t mTrackPos = 0;
	uint16_t mCRC = 0;
	int32_t mCellFracAccum = 128;
};

ScriptEngine::ScriptEngine(RawDisk& raw_disk)
	: mRawDisk(raw_disk)
{
	// Initialize the raw disk.
	for(auto& side : mRawDisk.mPhysTracks) {
		for(auto& track : side) {
			// Currently we use 25ns (SCP).
			track.mSamplesPerRev = 8333333;

			track.mSpliceStart = -1;
			track.mSpliceEnd = -1;
		}
	}
}

void ScriptEngine::EmitByte(bool special, uint8_t c) {
	if (!mpCurrentTrack)
		fatalf("Cannot emit data byte outside of a track.\n");

	mCRC = ComputeCRC(&c, 1, mCRC);

	// 4us = 160 ticks at 25ns
	uint8_t clockBits = special ? 0xC7 : 0xFF;
	uint8_t dataBits = c;

	for(int bit = 0; bit < 8; ++bit) {
		// clock bit
		if (clockBits & 0x80)
			mpCurrentTrack->mTransitions.push_back(mTrackPos + 80);

		clockBits += clockBits;

		mTrackPos += 160;

		// data bit
		if (dataBits & 0x80)
			mpCurrentTrack->mTransitions.push_back(mTrackPos + 80);

		dataBits += dataBits;

		mTrackPos += 160;
	}
}

void ScriptEngine::EmitPadBits(uint32_t count, bool set) {
	while(count-- > 0) {
		mpCurrentTrack->mTransitions.push_back(mTrackPos + 80);

		mTrackPos += 160;

		if (set)
			mpCurrentTrack->mTransitions.push_back(mTrackPos + 80);

		mTrackPos += 160;
	}
}

void ScriptEngine::EmitCellDelay(uint32_t count256) {
	mCellFracAccum += count256;

	int32_t delay = mCellFracAccum >> 8;

	if (delay < 1)
		delay = 1;

	mCellFracAccum -= delay << 8;

	mTrackPos += delay;
	mpCurrentTrack->mTransitions.push_back(mTrackPos);
}

void ScriptEngine::EmitCellDelayNoFlux(uint32_t count256) {
	mCellFracAccum += count256;

	int32_t delay = mCellFracAccum >> 8;

	mCellFracAccum -= delay << 8;

	mTrackPos += delay;
}

void ScriptEngine::BeginCRC() {
	mCRC = 0xFFFF;
}

void ScriptEngine::EndCRC() {
	// EmitByte() itself changes the CRC, so we need to cache it!
	uint16_t crc = mCRC;

	EmitByte(false, (uint8_t)(crc >> 8));
	EmitByte(false, (uint8_t)crc);
}

void ScriptEngine::BeginTrack(int track, int side) {
	if (track < 0 || track > 84 / mRawDisk.mTrackStep)
		fatalf("Invalid track number: %d\n", track);

	if (side < 0 || side >= mRawDisk.mSideCount)
		fatalf("Invalid side number: %d\n", side);

	mpCurrentTrack = &mRawDisk.mPhysTracks[0][track * mRawDisk.mTrackStep];
	mCurrentLogicalTrackNum = track;

	mpCurrentTrack->mIndexTimes.assign( { 0, 8333333, 1666666 } );

	mTrackPos = 0;
}

void ScriptEngine::EndTrack() {
	// Fill out the remaining time until the end of the track with $FF data
	// bytes.
	auto& transitions = mpCurrentTrack->mTransitions;
	const uint32_t endPos = mpCurrentTrack->mIndexTimes[1];

	if (mTrackPos > endPos) {
		const float kTicksToBits = 1.0f / 160.0f;

		printf("Warning: Overrun on track %d (%.1f bit cells > %.1f bit cells). Track will be truncated.\n", mCurrentLogicalTrackNum, (float)mTrackPos * kTicksToBits, (float)endPos * kTicksToBits);

		auto it = std::lower_bound(transitions.begin(), transitions.end(), endPos);

		transitions.erase(it, transitions.end());

		mTrackPos = endPos;
	}

	mpCurrentTrack->mSpliceStart = mTrackPos;
	mpCurrentTrack->mSpliceEnd = endPos;

	while(mTrackPos + 160 < endPos) {
		transitions.push_back(mTrackPos + 80);
		mTrackPos += 160;
	}

	// Now create another copy of the track, offset by a rotation.
	size_t len = transitions.size();

	transitions.resize(len * 2);
	std::transform(transitions.begin(), transitions.begin() + len, transitions.begin() + len, [endPos](uint32_t t) { return t + endPos; });

	mpCurrentTrack = nullptr;
}

void ScriptEngine::SetGeometry(int tracks, int sides) {
	if (tracks < 1 || tracks > 84)
		fatalf("Invalid track count: %u.\n", tracks);

	if (sides < 1 || sides > 2)
		fatalf("Invalid side count: %u.\n", sides);

	mRawDisk.mTrackCount = tracks;
	mRawDisk.mTrackStep = tracks < 42 ? 2 : 1;
	mRawDisk.mSideCount = sides;
}

///////////////////////////////////////////////////////////////////////////

class ScriptExpr {
public:
	virtual int32_t Evaluate(ScriptEngine& eng) = 0;
};

class ScriptExprValue final : public ScriptExpr {
public:
	ScriptExprValue(int32_t val) : mValue(val) {}

	int32_t Evaluate(ScriptEngine& eng) override { return mValue; }

private:
	const int32_t mValue;
};

class ScriptStatement {
public:
	virtual void Execute(ScriptEngine& eng) = 0;
};

class ScriptStatementBlock final : public ScriptStatement {
public:
	ScriptStatementBlock(ScriptStatement *const *list, size_t n)
		: mpStatements(list)
		, mCount(n)
	{
	}

	void Execute(ScriptEngine& eng) {
		for(size_t i=0; i<mCount; ++i)
			mpStatements[i]->Execute(eng);
	}

private:
	ScriptStatement *const *const mpStatements;
	const size_t mCount;
};

class ScriptStatementTrack final : public ScriptStatement {
public:
	ScriptStatementTrack(ScriptExpr *trackExpr, ScriptExpr *sideExpr, ScriptStatement *child)
		: mpTrackExpr(trackExpr)
		, mpSideExpr(sideExpr)
		, mpChildStatement(child)
	{
	}

	void Execute(ScriptEngine& eng) override {
		int32_t track = mpTrackExpr->Evaluate(eng);
		int32_t side = mpSideExpr ? mpSideExpr->Evaluate(eng) : 0;

		eng.BeginTrack(track, side);

		mpChildStatement->Execute(eng);

		eng.EndTrack();
	}

private:
	ScriptExpr *mpTrackExpr;
	ScriptExpr *mpSideExpr;
	ScriptStatement *mpChildStatement;
};

class ScriptStatementRepeat final : public ScriptStatement {
public:
	ScriptStatementRepeat(ScriptExpr *countExpr, ScriptStatement *child)
		: mpCountExpr(countExpr)
		, mpChildStatement(child)
	{
	}

	void Execute(ScriptEngine& eng) override {
		int32_t count = mpCountExpr->Evaluate(eng);

		while(count-- > 0)
			mpChildStatement->Execute(eng);
	}

private:
	ScriptExpr *mpCountExpr;
	ScriptStatement *mpChildStatement;
};

class ScriptStatementByte final : public ScriptStatement {
public:
	ScriptStatementByte(bool special, ScriptExpr *e) : mbSpecial(special), mpValueExpr(e) {}

	void Execute(ScriptEngine& eng) override {
		int32_t v = mpValueExpr->Evaluate(eng);
		if (v < 0 || v > 255)
			fatalf("Invalid data byte: %d\n", (int)v);

		eng.EmitByte(mbSpecial, (uint8_t)v);
	};

private:
	const bool mbSpecial;
	ScriptExpr *const mpValueExpr;
};

class ScriptStatementBytes final : public ScriptStatement {
public:
	ScriptStatementBytes(const uint8_t *data, size_t len) : mpData(data), mLen(len) {}

	void Execute(ScriptEngine& eng) override {
		for(size_t i=0; i<mLen; ++i)
			eng.EmitByte(false, mpData[i]);
	};

private:
	const uint8_t *const mpData;
	const size_t mLen;
};

class ScriptStatementPadBits final : public ScriptStatement {
public:
	ScriptStatementPadBits(ScriptExpr *e, ScriptExpr *e2) : mpCountExpr(e), mpValueExpr(e2) {}

	void Execute(ScriptEngine& eng) override {
		int32_t count = mpCountExpr->Evaluate(eng);
		if (count < 0 || count > 1000000)
			fatalf("Invalid pad bit count: %d\n", (int)count);

		int32_t v = mpValueExpr->Evaluate(eng);
		if (v < 0 || v > 1)
			fatalf("Invalid pad bit value: %d\n", (int)v);

		eng.EmitPadBits(count, v != 0);
	};

private:
	ScriptExpr *const mpCountExpr;
	ScriptExpr *const mpValueExpr;
};

class ScriptStatementCRCBegin final : public ScriptStatement {
public:
	void Execute(ScriptEngine& eng) override {
		eng.BeginCRC();
	}
};

class ScriptStatementCRCEnd final : public ScriptStatement {
public:
	void Execute(ScriptEngine& eng) override {
		eng.EndCRC();
	}
};

class ScriptStatementFlux final : public ScriptStatement {
public:
	ScriptStatementFlux(ScriptExpr *e) : mpCountExpr(e) {}

	void Execute(ScriptEngine& eng) override {
		int32_t count = mpCountExpr->Evaluate(eng);
		if (count < 1 || count > 1000000)
			fatalf("Invalid cell delay: %d\n", (int)count);

		eng.EmitCellDelay((count * 160 * 256 + 50) / 100);
	};

private:
	ScriptExpr *const mpCountExpr;
};

class ScriptStatementNoFlux final : public ScriptStatement {
public:
	ScriptStatementNoFlux(ScriptExpr *e) : mpCountExpr(e) {}

	void Execute(ScriptEngine& eng) override {
		int32_t count = mpCountExpr->Evaluate(eng);
		if (count < 1 || count > 1000000)
			fatalf("Invalid cell delay: %d\n", (int)count);

		eng.EmitCellDelayNoFlux((count * 160 * 256 + 50) / 100);
	};

private:
	ScriptExpr *const mpCountExpr;
};

class ScriptStatementGeometry final : public ScriptStatement {
public:
	ScriptStatementGeometry(ScriptExpr *e, ScriptExpr *e2) : mpTracksExpr(e), mpSidesExpr(e) {}

	void Execute(ScriptEngine& eng) override {
		int32_t tracks = mpTracksExpr->Evaluate(eng);
		int32_t sides = mpSidesExpr->Evaluate(eng);

		if (tracks < 1 || tracks > 84)
			fatalf("Invalid track count: %d\n", (int)tracks);

		if (sides < 1 || sides > 2)
			fatalf("Invalid side count: %d\n", (int)sides);

		eng.SetGeometry(tracks, sides);
	};

private:
	ScriptExpr *const mpTracksExpr;
	ScriptExpr *const mpSidesExpr;
};

class ScriptCompiler {
	ScriptCompiler(const ScriptCompiler&) = delete;
	ScriptCompiler& operator=(const ScriptCompiler&) = delete;

public:
	ScriptCompiler() = default;
	~ScriptCompiler();

	void Run(const char *fn, const void *text, size_t len, RawDisk& rawDisk);

private:
	enum {
		kTokEOF = 0,
		kTokError,

		kTokInt = 128,
		kTokTrack,
		kTokRepeat,
		kTokByte,
		kTokBytes,
		kTokSpecialByte,
		kTokPadBits,
		kTokCRCBegin,
		kTokCRCEnd,
		kTokFlux,
		kTokNoFlux,
		kTokGeometry
	};

	ScriptStatement *ParseStatement();
	ScriptStatement *ParseChildStatement();
	ScriptExpr *ParseExpression();

	void Push(int tok);
	int Token();
	bool Error(const char *str);
	bool ErrorF(const char *format, ...);

	template<class T, typename... Args>
	T *Alloc(Args&&... args) {
		void *mem = AllocRaw(sizeof(T));

		return new(mem) T(std::forward<Args>(args)...);
	}

	void *AllocRaw(size_t n) {
		mAllocs.push_back(nullptr);

		void *p = malloc(n);
		if (!p) {
			mAllocs.pop_back();
			throw std::bad_alloc();
		}

		mAllocs.back() = p;
		return p;
	}

	int mPushedToken = -1;
	const char *mpSrc = nullptr;
	const char *mpSrcEnd = nullptr;
	const char *mpFileName = nullptr;
	const char *mpTokenStart = nullptr;
	const char *mpLineStart = nullptr;
	int mLineNo = 1;
	int32_t mIntVal = 0;

	std::vector<void *> mAllocs;
};

ScriptCompiler::~ScriptCompiler() {
	while(!mAllocs.empty()) {
		free(mAllocs.back());
		mAllocs.pop_back();
	}
}

void ScriptCompiler::Run(const char *fn, const void *text, size_t len, RawDisk& rawDisk) {
	mpSrc = (const char *)text;
	mpSrcEnd = mpSrc + len;
	mpFileName = fn;
	mpLineStart = mpSrc;

	std::vector<ScriptStatement *> children;

	for(;;) {
		int tok = Token();

		if (tok == kTokEOF || tok == kTokError)
			break;

		Push(tok);

		ScriptStatement *c = ParseStatement();
		if (!c)
			break;

		children.push_back(c);
	}

	if (mPushedToken == kTokError)
		fatalf("Script compilation failed.\n");

	ScriptEngine eng(rawDisk);
	for(auto *s : children)
		s->Execute(eng);
}

ScriptStatement *ScriptCompiler::ParseStatement() {
	int tok = Token();

	if (tok == kTokError)
		return nullptr;

	ScriptStatement *s = nullptr;

	if (tok == kTokTrack) {
		ScriptExpr *e = ParseExpression();
		if (!e)
			return nullptr;

		ScriptExpr *e2 = nullptr;
		tok = Token();
		if (tok == ',') {
			e2 = ParseExpression();
			if (!e2)
				return nullptr;
		} else
			Push(tok);

		ScriptStatement *c = ParseChildStatement();
		if (!c)
			return nullptr;

		return Alloc<ScriptStatementTrack>(e, e2, c);
	} else if (tok == kTokRepeat) {
		ScriptExpr *e = ParseExpression();
		if (!e)
			return nullptr;

		ScriptStatement *c = ParseChildStatement();
		if (!c)
			return nullptr;

		return Alloc<ScriptStatementRepeat>(e, c);
	} else if (tok == kTokByte) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		s = Alloc<ScriptStatementByte>(false, e);
	} else if (tok == kTokBytes) {	
		std::vector<uint8_t> data;

		for(;;) {
			tok = Token();
			if (tok == kTokError)
				return nullptr;

			if (tok != kTokInt) {
				Error("Expected integral constant");
				return nullptr;
			}

			if (mIntVal < 0 || mIntVal > 255) {
				Error("Value out of range (must be 0-255)");
				return nullptr;
			}

			data.push_back((uint8_t)mIntVal);

			tok = Token();

			if (tok == ';')
				break;

			if (tok != ',') {
				Error("Expected ',' or end of statement");
				return nullptr;
			}
		}

		size_t n = data.size();
		uint8_t *buf = (uint8_t *)AllocRaw(n);

		memcpy(buf, data.data(), n);

		return Alloc<ScriptStatementBytes>(buf, n);
	} else if (tok == kTokSpecialByte) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		s = Alloc<ScriptStatementByte>(true, e);
	} else if (tok == kTokPadBits) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		tok = Token();
		if (tok != ',') {
			if (tok != kTokError)
				Error("Expected ','");
			return nullptr;
		}

		auto *e2 = ParseExpression();
		if (!e2)
			return nullptr;

		s = Alloc<ScriptStatementPadBits>(e, e2);
	} else if (tok == kTokCRCBegin) {
		s = Alloc<ScriptStatementCRCBegin>();
	} else if (tok == kTokCRCEnd) {
		s = Alloc<ScriptStatementCRCEnd>();
	} else if (tok == kTokFlux) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		s = Alloc<ScriptStatementFlux>(e);
	} else if (tok == kTokNoFlux) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		s = Alloc<ScriptStatementNoFlux>(e);
	} else if (tok == kTokGeometry) {
		auto *e = ParseExpression();
		if (!e)
			return nullptr;

		tok = Token();
		if (tok != ',') {
			Error("Expected side count after track count");
			return nullptr;
		}
		auto *e2 = ParseExpression();
		if (!e2)
			return nullptr;

		s = Alloc<ScriptStatementGeometry>(e, e2);
	} else {
		Error("Expected statement");
		return nullptr;
	}

	tok = Token();
	if (tok == kTokError)
		return nullptr;

	if (tok != ';') {
		Error("Expected ';' at end of statement");
		return nullptr;
	}

	return s;
}

ScriptStatement *ScriptCompiler::ParseChildStatement() {
	int tok = Token();

	if (tok == ':')
		return ParseStatement();
	else if (tok == '{') {
		std::vector<ScriptStatement *> children;

		for(;;) {
			int tok = Token();

			if (tok == '}')
				break;

			Push(tok);

			ScriptStatement *c = ParseStatement();
			if (!c)
				return nullptr;

			children.push_back(c);
		}

		if (children.size() == 0)
			return Alloc<ScriptStatementBlock>(nullptr, 0);
		else if (children.size() == 1)
			return children.front();
		else {
			auto list = (ScriptStatement **)AllocRaw(sizeof(ScriptStatement *) * children.size());

			std::copy(children.begin(), children.end(), list);

			return Alloc<ScriptStatementBlock>(list, children.size());
		}
	}
	
	if (tok != kTokError)
		Error("Expected child statement");

	return nullptr;
}

ScriptExpr *ScriptCompiler::ParseExpression() {
	int tok = Token();

	if (tok == kTokInt)
		return Alloc<ScriptExprValue>(mIntVal);

	if (tok != kTokError)
		Error("Expected value");

	return nullptr;
}

void ScriptCompiler::Push(int tok) {
	mPushedToken = tok;
}

int ScriptCompiler::Token() {
	if (mPushedToken >= 0) {
		int tok = mPushedToken;
		if (tok != kTokError)
			mPushedToken = -1;
		return tok;
	}

	// skip whitespace
	mpTokenStart = mpSrc;

	char c;
	for(;;) {
		if (mpSrc == mpSrcEnd) {
			return kTokEOF;
		}

		c = *mpSrc++;
		if (c == '/' && mpSrc != mpSrcEnd) {
			if (*mpSrc == '*') {
				++mpSrc;

				for(;;) {
					if (mpSrc == mpSrcEnd) {
						Error("Unterminated multi-line comment");
						return kTokError;
					}

					c = *mpSrc++;

					if (c == '*') {
						if (mpSrc != mpSrcEnd && *mpSrc == '/') {
							++mpSrc;
							break;
						}
					}
				}

				continue;
			} else if (*mpSrc == '/') {
				++mpSrc;

				while(mpSrc != mpSrcEnd && *mpSrc != '\r' && *mpSrc != '\n')
					++mpSrc;

				continue;
			}
		}

		if (c == '\r' || c == '\n') {
			if (mpSrc != mpSrcEnd && *mpSrc == (c ^ ('\r' ^ '\n')))
				++mpSrc;

			mpLineStart = mpSrc;
			++mLineNo;
		} else if (c != ' ' && c != '\t')
			break;
	}

	mpTokenStart = mpSrc - 1;

	if (strchr(":;{},", c))
		return (int)c;

	if (c == '0' && mpSrc != mpSrcEnd && (mpSrc[0] == 'x' || mpSrc[0] == 'X')) {
		bool validHex = false;

		mIntVal = 0;

		++mpSrc;
		while (mpSrc != mpSrcEnd) {
			int digit = 0;

			c = *mpSrc;
			if (c >= '0' && c <= '9')
				digit = (int)(c - '0');
			else if (c >= 'a' && c <= 'f')
				digit = (int)(c - 'a') + 10;
			else if (c >= 'A' && c <= 'F')
				digit = (int)(c - 'A') + 10;
			else
				break;

			++mpSrc;

			validHex = true;

			if (mIntVal > 0x7FFFFFF) {
				Error("Integral constant too big");
				return kTokError;
			}

			mIntVal = mIntVal * 16 + digit;

			if (mpSrc == mpSrcEnd)
				break;

		}

		if (!validHex) {
			Error("Invalid hex constant");
			return kTokError;
		}

		return kTokInt;
	} else if (c >= '0' && c <= '9') {
		mIntVal = 0;

		do {
			if (mIntVal > 214748364) {
				Error("Integral constant too big");
				return kTokError;
			}

			mIntVal = mIntVal * 10;

			int32_t digit = (int32_t)(c - '0');
			if (2147483647 - mIntVal < digit) {
				Error("Integral constant too big");
				return kTokError;
			}

			mIntVal += digit;

			if (mpSrc == mpSrcEnd)
				break;

			c = *mpSrc++;
		} while(c >= '0' && c <= '9');

		--mpSrc;
		return kTokInt;
	}

	if (isalpha((unsigned char)c)) {
		while(mpSrc != mpSrcEnd && (isalnum((unsigned char)*mpSrc) || *mpSrc == '_'))
			++mpSrc;

		int len = (int)(mpSrc - mpTokenStart);

		switch(len) {
			case 4:
				if (!memcmp(mpTokenStart, "byte", 4))
					return kTokByte;
				else if (!memcmp(mpTokenStart, "flux", 4))
					return kTokFlux;
				break;

			case 5:
				if (!memcmp(mpTokenStart, "bytes", 5))
					return kTokBytes;
				else if (!memcmp(mpTokenStart, "track", 5))
					return kTokTrack;
				break;

			case 6:
				if (!memcmp(mpTokenStart, "repeat", 6))
					return kTokRepeat;
				break;

			case 7:
				if (!memcmp(mpTokenStart, "crc_end", 7))
					return kTokCRCEnd;
				else if (!memcmp(mpTokenStart, "no_flux", 7))
					return kTokNoFlux;
				break;

			case 8:
				if (!memcmp(mpTokenStart, "pad_bits", 8))
					return kTokPadBits;
				else if (!memcmp(mpTokenStart, "geometry", 8))
					return kTokGeometry;
				break;

			case 9:
				if (!memcmp(mpTokenStart, "crc_begin", 9))
					return kTokCRCBegin;
				break;

			case 12:
				if (!memcmp(mpTokenStart, "special_byte", 12))
					return kTokSpecialByte;
				break;
		}

		ErrorF("Unrecognized keyword: '%.*s'", len, mpTokenStart);
		return kTokError;
	}

	if (c >= 0x20 && c < 0x7F)
		ErrorF("Unrecognized character '%c'", (int)(char)c);
	else
		ErrorF("Unrecognized character 0x%02X", (int)(unsigned char)c);

	return kTokError;
}

bool ScriptCompiler::Error(const char *str) {
	printf("%s(%d,%d): Error: %s\n", mpFileName, mLineNo, (int)(mpTokenStart - mpLineStart), str);
	mPushedToken = kTokError;
	return false;
}

bool ScriptCompiler::ErrorF(const char *format, ...) {
	printf("%s(%d,%d): Error: ", mpFileName, mLineNo, (int)(mpTokenStart - mpLineStart));
	va_list val;
	va_start(val, format);
	vprintf(format, val);
	va_end(val);
	putchar('\n');
	mPushedToken = kTokError;
	return false;
}

void script_read(RawDisk& raw_disk, const char *path, int selected_track) {
	FILE *f = fopen(path, "rb");

	if (!f)
		fatalf("Unable to open disk script: %s\n", path);

	if (fseek(f, 0, SEEK_END))
		fatalf("Unable to read disk script: %s\n", strerror(ferror(f)));

	auto len = ftell(f);
	if (len < 0)
		fatalf("Unable to read disk script: %s\n", strerror(ferror(f)));

	if (fseek(f, 0, SEEK_SET))
		fatalf("Unable to read disk script: %s\n", strerror(ferror(f)));

	if (len > 0x1000000 || len != (size_t)len)
		fatalf("Disk script is too big: %lld\n", (long long)len);

	void *buf = malloc((size_t)len);
	if (1 != fread(buf, (size_t)len, 1, f))
		fatalf("Unable to read disk script: %s\n", strerror(ferror(f)));
	
	fclose(f);

	ScriptCompiler eng;
	eng.Run(path, buf, (size_t)len, raw_disk);

	free(buf);

	raw_disk.mSynthesized = true;
}
