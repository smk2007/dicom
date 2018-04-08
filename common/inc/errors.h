////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///  Errors.h
///
////////////////////////////////////////////////////////////////////////////////////////////////////////


// Common MACROS
#define RETURN_IF_EQUALS_VALUE(ret, cond, value) 	                    \
	do { 											                    \
		auto RETURN_IF_EQUALS_VALUE_1 = cond;							\
		auto RETURN_IF_EQUALS_VALUE_2 = value;							\
		if (RETURN_IF_EQUALS_VALUE_1 == RETURN_IF_EQUALS_VALUE_2)		\
		{											                    \
			return ret;								                    \
		}											                    \
	} while(0)

#define RETURN_HR_IF_FAILED(ret, hr)						                    \
	do { 													                    \
		auto RETURN_HR_IF_FAILED_1 = hr;										\
		auto RETURN_HR_IF_FAILED_2 = ret;										\
		RETURN_IF_EQUALS_VALUE(RETURN_HR_IF_FAILED_2, SUCCEEDED(RETURN_HR_IF_FAILED_1), false);	\
	} while(0)

// Common FAILFAST MACROS
#define FAIL_FAST_IF_EQUALS_VALUE(cond, value) 		                        \
	do { 											                        \
		auto FAIL_FAST_IF_EQUALS_VALUE_1 = cond;							\
		auto FAIL_FAST_IF_EQUALS_VALUE_2 = value;							\
		if (FAIL_FAST_IF_EQUALS_VALUE_1 == FAIL_FAST_IF_EQUALS_VALUE_2)		\
		{											                        \
			__fastfail(0);							                        \
		}											                        \
	} while(0)

// MACROS that return HRESULTS
#define RETURN_IF_FAILED(hr) 			\
	do { 								\
		auto RETURN_IF_FAILED_1 = hr;	\
		RETURN_HR_IF_FAILED(RETURN_IF_FAILED_1, RETURN_IF_FAILED_1);	\
	} while(0)

#define RETURN_HR_IF_NULL(hr, value)			\
	do { 										\
		auto RETURN_HR_IF_NULL_1 = hr;					\
		auto RETURN_HR_IF_NULL_2 = value == nullptr;	\
		RETURN_IF_EQUALS_VALUE(RETURN_HR_IF_NULL_1, RETURN_HR_IF_NULL_2, true);	\
	} while(0)

#define RETURN_HR_IF(hr, value) RETURN_IF_EQUALS_VALUE(hr, value, true)
#define RETURN_HR_IF_FALSE(hr, value) RETURN_IF_EQUALS_VALUE(hr, value, false)

// MACROS that return boolean
#define RETURN_FALSE_IF_FAILED(hr) 								\
	do { 														\
		auto RETURN_FALSE_IF_FAILED_1 = hr;											\
		RETURN_IF_EQUALS_VALUE(false, SUCCEEDED(RETURN_FALSE_IF_FAILED_1), false);	\
	} while(0)

#define RETURN_TRUE_IF_FAILED(hr) 								\
	do { 														\
		auto RETURN_TRUE_IF_FAILED_1 = hr;											\
		RETURN_IF_EQUALS_VALUE(true, SUCCEEDED(RETURN_TRUE_IF_FAILED_1), false);	\
	} while(0)

#define RETURN_FALSE_IF_NULL(value)					\
	do { 											\
		auto RETURN_FALSE_IF_NULL_1 = value == nullptr;					\
		RETURN_IF_EQUALS_VALUE(false, RETURN_FALSE_IF_NULL_1, true);	\
	} while(0)

#define RETURN_TRUE_IF_NULL(value)					\
	do { 											\
		auto RETURN_TRUE_IF_NULL_1 = value == nullptr;				\
		RETURN_IF_EQUALS_VALUE(true, RETURN_TRUE_IF_NULL_1, true);	\
	} while(0)


// MACROS that return null
#define RETURN_NULL_IF_FAILED(hr) 								\
	do { 														\
		auto RETURN_NULL_IF_FAILED_1 = hr;											\
		RETURN_IF_EQUALS_VALUE(nullptr, SUCCEEDED(RETURN_NULL_IF_FAILED_1), false);	\
	} while(0)

#define RETURN_NULL_IF_NULL(value)					\
	do { 											\
		auto RETURN_NULL_IF_NULL_1 = value == nullptr;					\
		RETURN_IF_EQUALS_VALUE(nullptr, RETURN_NULL_IF_NULL_1, true);	\
	} while(0)


// MACROS that failfast
#define FAIL_FAST_IF_FAILED(hr) 	FAIL_FAST_IF_EQUALS_VALUE(SUCCEEDED(hr), false) 
#define FAIL_FAST_IF_NULL(obj) 		FAIL_FAST_IF_EQUALS_VALUE(obj, nullptr) 
#define FAIL_FAST_IF_FALSE(cond) 	FAIL_FAST_IF_EQUALS_VALUE(cond, false) 
#define FAIL_FAST_IF_TRUE(cond) 	FAIL_FAST_IF_EQUALS_VALUE(cond, true) 
#define FAIL_FAST_IF(cond) 			FAIL_FAST_IF_TRUE(cond) 