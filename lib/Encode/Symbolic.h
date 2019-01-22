/*
 * Symbolic.h
 *
 *  Created on: Oct 22, 2018
 *      Author: klee
 */

#ifndef LIB_ENCODE_SYMBOLIC_H_
#define LIB_ENCODE_SYMBOLIC_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../../include/klee/ExecutionState.h"
#include "../../include/klee/Expr.h"
#include "../../include/klee/util/Ref.h"

namespace llvm {
class Function;
} /* namespace llvm */

#define BIT_WIDTH 64

using namespace llvm;

namespace klee {

class Symbolic {
public:
	Symbolic(Executor *executor);
	virtual ~Symbolic();

private:
	std::stringstream ss;
	std::map<uint64_t, unsigned> loadRecord;
	std::map<uint64_t, unsigned> storeRecord;
	Executor* executor;
	std::string json;

public:
	void load(ExecutionState &state, KInstruction *ki);
	void call(ExecutionState &state, KInstruction *ki, Function *function,
			std::vector<ref<Expr> > &arguments);
	void callReturnValue(ExecutionState &state, KInstruction *ki, Function *function);

private:
	ref<Expr> manualMakeSymbolic(std::string name, unsigned size);
	ref<Expr> readExpr(ExecutionState &state, ref<Expr> address,
			Expr::Width size);
	unsigned getLoadTime(uint64_t address);
	unsigned getStoreTime(uint64_t address);
	std::string createGlobalVarFullName(std::string i, unsigned memoryId,
			uint64_t address, bool isGlobal, unsigned time, bool isStore);

};

} /* namespace klee */

#endif /* LIB_ENCODE_SYMBOLIC_H_ */
