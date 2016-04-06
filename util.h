#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>
#include <functional>
#include <type_traits>

typedef std::basic_string<uint8_t> Datastring;
typedef std::basic_string<uint16_t> Datastring16;

enum Command {
	CORE_INST = 0,
	SHIFT_OUT_TABLAT = 2,
	TABLE_READ = 8,
	TABLE_READ_post_inc = 9,
	TABLE_READ_post_dec = 10,
	TABLE_READ_pre_inc = 11,
	TABLE_WRITE = 12,
	TABLE_WRITE_post_inc2 = 13,
	TABLE_WRITE_post_inc2_start_pgm = 14,
	TABLE_WRITE_start_pgm = 15,
};

enum Pins {
	nMCLR = (1<<0),
	PGM = (1<<1),
	PGC = (1<<2),
	PGD = (1<<3),
};

typedef uint64_t Duration;
static inline Duration MilliSeconds(uint64_t x) { return x * 1000000; }
static inline Duration MicroSeconds(uint64_t x) { return x * 1000; }
static inline Duration NanoSeconds(uint64_t x) { return x; }

void Sleep(Duration duration);

#ifdef __GNUC__
void fatal(const char *fmt, ...) __attribute__((format (printf, 1, 2))) __attribute__((noreturn));
#else
/*@noreturn@*/ void fatal(const char *fmt, ...);
#endif
#define FATAL(fmt, ...) fatal("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__)

class AutoClosureRunner {
public:
	AutoClosureRunner(std::function<void()> func) : func_(func) {}
	~AutoClosureRunner() { func_(); }
private:
	std::function<void()> func_;
};

template <class C, class T>
typename std::enable_if<!std::is_pod<T>::value,bool>::type ContainsKey(const C &c, const T &t) {
	return c.find(t) != c.end();
}
template <class C, class T>
typename std::enable_if<std::is_pod<T>::value,bool>::type ContainsKey(const C &c, T t) {
	return c.find(t) != c.end();
}

#endif
