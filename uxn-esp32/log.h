#define LOG(...) 										\
	do {												\
		Serial.printf("%s:%d: ", __FILE__, __LINE__); 	\
		Serial.printf(__VA_ARGS__); 					\
	} while(0)