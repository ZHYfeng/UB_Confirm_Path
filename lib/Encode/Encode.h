/*
 * Encode.h
 *
 *  Created on: Oct 22, 2018
 *      Author: yhao
 */

#ifndef LIB_ENCODE_ENCODE_H_
#define LIB_ENCODE_ENCODE_H_

#include <llvm/ADT/StringRef.h>
#include <z3++.h>
#include <map>
#include <string>
#include <vector>

#include "../../include/klee/Expr.h"
#include "../../include/klee/util/Ref.h"
#include "KQuery2Z3.h"

using namespace z3;

namespace klee {

class Encode {
public:
	Encode();
	Encode(const Encode &e);
	virtual ~Encode();

public:
	context *z3_ctxx;
	solver z3_solverr;
	KQuery2Z3 kq;
	std::vector<ref<Expr>> constraintExpr;
	std::vector<expr> constraintexpr;
	std::vector<bool> path;
	std::vector<std::string> globalname;
	std::vector<ref<Expr>> globalexpr;
	unsigned int flag;
	std::string Json;
	std::vector<std::string> whiteList;
	std::vector<std::string> blackList;
	std::vector<std::string> useList;
	std::vector<std::string> BBName;
	std::vector<int> BBCount;
	std::vector<int> isWhiteList;

public:
	void addList();
	void addBrConstraint(ref<Expr> cond, bool isTrue, llvm::StringRef labelTrue,
			llvm::StringRef labelFalse);
	void checkWhiteList(llvm::StringRef label);
	void checkBlackList(llvm::StringRef label);
	void checkUseList(llvm::StringRef label);
	void checkBBCount(llvm::StringRef label);
	int checkList(llvm::StringRef label);
	static std::string getName(ref<Expr> value);
	static bool IsRet(std::string globalVarFullName);
	static std::string getSymbolicName(ref<Expr> value);
	static void resolveSymbolicExpr(ref<Expr> value,
			std::set<std::string> &relatedSymbolicExpr);
};

}

#endif /* LIB_ENCODE_ENCODE_H_ */
