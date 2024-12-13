#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include "HulaScript.hpp"
#include <tuple>
#include <stdexcept>
#include "HulaScript.hpp"

using namespace HulaScript;

static size_t gcd(size_t a, size_t b) {
	while (b != 0) {
		std::tie(a, b) = std::make_tuple(b, a % b);
	}
	return a;
}

static size_t lcm(size_t a, size_t b) {
	if (a > b) {
		return (a / gcd(a, b)) * b;
	}
	else {
		return (b / gcd(a, b)) * a;
	}
}

void instance::handle_rational_add(value& a, value& b) {
	if (b.data.id == 0) {
		evaluation_stack.push_back(a);
		return;
	}

	size_t denom = lcm(a.function_id, b.function_id);
	size_t anum = a.data.id * (denom / a.function_id);
	size_t bnum = b.data.id * (denom / b.function_id);

	if ((a.flags & value::vflags::RATIONAL_IS_NEGATIVE) == (b.flags & value::vflags::RATIONAL_IS_NEGATIVE)) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, a.flags, denom, anum + bnum));
	}
	else if(anum > bnum) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, a.flags, denom, anum - bnum));
	}
	else {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, b.flags, denom, bnum - anum));
	}
}

void instance::handle_rational_subtract(value& a, value& b) {
	if (b.data.id == 0) {
		evaluation_stack.push_back(a);
		return;
	}

	size_t denom = lcm(a.function_id, b.function_id);
	size_t anum = a.data.id * (denom / a.function_id);
	size_t bnum = b.data.id * (denom / b.function_id);

	if ((a.flags & value::vflags::RATIONAL_IS_NEGATIVE) != (b.flags & value::vflags::RATIONAL_IS_NEGATIVE)) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, a.flags, denom, anum + bnum));
	}
	else if (anum > bnum) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, a.flags, denom, anum - bnum));
	}
	else {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, (a.flags & value::vflags::RATIONAL_IS_NEGATIVE) ? value::value::NONE : value::vflags::RATIONAL_IS_NEGATIVE, denom, bnum - anum));
	}
}

void instance::handle_rational_multiply(value& a, value& b) {
	size_t anum_bdenom_gcd = gcd(a.data.id, b.function_id);
	size_t bnum_adenom_gcd = gcd(b.data.id, a.function_id);
	size_t anum = a.data.id / anum_bdenom_gcd;
	size_t bnum = b.data.id / bnum_adenom_gcd;
	size_t adenom = a.function_id / bnum_adenom_gcd;
	size_t bdenom = b.function_id / anum_bdenom_gcd;

	if (anum == 0 || bnum == 0) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, value::vflags::NONE, 1, 0));
		return;
	}
	if (anum > SIZE_MAX / bnum) {
		panic("Overflow: Numerator in rational multiplication is too large.");
	}
	if (adenom > UINT32_MAX / bdenom) {
		panic("Overflow: Denominator in rational multiplication is too large.");
	}

	value::vflags flags = ((a.flags & value::vflags::RATIONAL_IS_NEGATIVE) == (b.flags & value::vflags::RATIONAL_IS_NEGATIVE)) ? value::vflags::NONE : value::vflags::RATIONAL_IS_NEGATIVE;

	evaluation_stack.push_back(value(value::vtype::RATIONAL, flags, adenom * bdenom, anum * bnum));
}

void instance::handle_rational_divide(value& a, value& b) {
	size_t anum_bdenom_gcd = gcd(a.data.id, b.data.id);
	size_t bnum_adenom_gcd = gcd(a.function_id, b.function_id);
	size_t anum = a.data.id / anum_bdenom_gcd;
	size_t bnum = b.data.id / anum_bdenom_gcd;
	size_t adenom = a.function_id / bnum_adenom_gcd;
	size_t bdenom = b.function_id / bnum_adenom_gcd;

	if (bnum == 0) {
		panic("Rational: Divide by Zero.");
	}
	if (anum > SIZE_MAX / bdenom) {
		panic("Overflow: Numerator in rational multiplication is too large.");
	}
	if (adenom > UINT32_MAX / bnum) {
		panic("Overflow: Denominator in rational multiplication is too large.");
	}

	value::vflags flags = ((a.flags & value::vflags::RATIONAL_IS_NEGATIVE) == (b.flags & value::vflags::RATIONAL_IS_NEGATIVE)) ? value::vflags::NONE : value::vflags::RATIONAL_IS_NEGATIVE;

	evaluation_stack.push_back(value(value::vtype::RATIONAL, flags, adenom * bnum, anum * bdenom));
}

void instance::handle_rational_exponentiate(value& a, value& b) {
	if (b.data.id == 0) {
		evaluation_stack.push_back(value(value::vtype::RATIONAL, value::vflags::NONE, 1, 1));
		return;
	}

	if (b.function_id == 1) {
		size_t num = 1;
		size_t denom = 1;

		for (size_t i = 0; i < b.data.id; i++)
		{
			if (num > SIZE_MAX / a.data.id || denom > UINT32_MAX / a.function_id) {
				size_t common_denom = gcd(num, denom);
				num /= common_denom;
				denom /= common_denom;
			}

			if (b.flags & value::vflags::RATIONAL_IS_NEGATIVE) {
				num *= a.function_id;
				denom *= a.data.id;
			}
			else {
				num *= a.data.id;
				denom *= a.function_id;
			}
		}

		value::vflags flags = (a.flags & value::vflags::RATIONAL_IS_NEGATIVE && b.data.id % 2 == 1) ? value::vflags::RATIONAL_IS_NEGATIVE : value::vflags::NONE;

		evaluation_stack.push_back(value(value::vtype::RATIONAL, flags, denom, num));
	}
	else {
		evaluation_stack.push_back(value(pow(a.number(*this), b.number(*this))));
	}
}

void HulaScript::instance::handle_rational_modulo(value& a, value& b) {
	size_t anum_bdenom_gcd = gcd(a.data.id, b.data.id);
	size_t bnum_adenom_gcd = gcd(a.function_id, b.function_id);
	size_t anum = a.data.id / anum_bdenom_gcd;
	size_t bnum = b.data.id / anum_bdenom_gcd;
	size_t adenom = a.function_id / bnum_adenom_gcd;
	size_t bdenom = b.function_id / bnum_adenom_gcd;

	if (bnum == 0) {
		panic("Rational: Divide by Zero.");
	}
	if (anum > SIZE_MAX / bdenom) {
		panic("Overflow: Numerator in rational multiplication is too large.");
	}
	if (adenom > UINT32_MAX / bnum) {
		panic("Overflow: Denominator in rational multiplication is too large.");
	}

	value::vflags flags = ((a.flags & value::vflags::RATIONAL_IS_NEGATIVE) == (b.flags & value::vflags::RATIONAL_IS_NEGATIVE)) ? value::vflags::NONE : value::vflags::RATIONAL_IS_NEGATIVE;

	size_t denom = adenom * bnum;
	size_t nom = (anum * bdenom) % denom;
	denom *= b.function_id;
	nom *= b.data.id;
	size_t common_denom = gcd(denom, nom);
	
	evaluation_stack.push_back(value(value::vtype::RATIONAL, flags, denom / common_denom, nom / common_denom));
}

instance::value instance::parse_rational(std::string str) const {
	uint64_t numerator = 0;
	uint32_t denominator = 1;

	bool is_negate = false;
	bool decimal_detected = false;
	for (size_t i = 0; i < str.size(); i++) {
		char c = str.at(i);
		if (c >= '0' && c <= '9') {
			if (numerator > UINT64_MAX / 10) {
				throw std::runtime_error("Overflow: Numerator is too large.");
			}

			numerator *= 10;
			numerator += (c - '0');

			if (decimal_detected) {
				if (denominator > UINT32_MAX / 10) {
					throw std::runtime_error("Overflow: Denominator is too large.");
				}
				denominator *= 10;
			}
		}
		else if (c == '.') {
			if (decimal_detected) {
				throw std::runtime_error("Format: Two decimals detected.");
			}

			decimal_detected = true;
		}
		else if (c == '-') {
			if (is_negate) {
				throw std::runtime_error("Format: Two negates detected.");
			}
			is_negate = true;
		}
		else {
			throw std::runtime_error("Format: Must be digit (0-9).");
		}
	}

	size_t common = gcd(denominator, numerator);
	return value(value::vtype::RATIONAL, is_negate ? value::vflags::RATIONAL_IS_NEGATIVE : value::vflags::NONE, denominator / common, numerator / common);
}