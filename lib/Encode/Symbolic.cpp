/*
 * Symbolic.cpp
 *
 *  Created on: Oct 22, 2018
 *      Author: klee
 */

#include "Symbolic.h"

#include <llvm/IR/CallSite.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>
#include <sstream>
#include <utility>

#include "../../include/klee/Internal/Module/KInstruction.h"
#include "../Core/AddressSpace.h"
#include "../Core/Executor.h"
#include "../Core/Memory.h"
#include "Encode.h"

namespace klee {

Symbolic::Symbolic(Executor *executor) {
	// TODO Auto-generated constructor stub
	this->executor = executor;
}

Symbolic::~Symbolic() {
	// TODO Auto-generated destructor stub
}

ref<Expr> Symbolic::manualMakeSymbolic(std::string name, unsigned int size) {
	//添加新的符号变量
	const Array *array = new Array(name, size);
	ObjectState *os = new ObjectState(size, array);
	ref<Expr> offset = ConstantExpr::create(0, BIT_WIDTH);
	ref<Expr> result = os->read(offset, size);
#if DEBUGINFO
	llvm::errs() << "make symboic:" << name << "\n";
	llvm::errs() << "result : ";
	result->dump();
#endif
	return result;
}

std::string Symbolic::createGlobalVarFullName(std::string i, unsigned memoryId,
		uint64_t address, bool isGlobal, unsigned time, bool isStore) {
	char signal;
	ss.str("");
	ss << i;
	if (isGlobal) {
		signal = 'G';
	} else {
		signal = 'L';
	}
	ss << signal;
	ss << memoryId;
	ss << '_';
	ss << address;
	if (isStore) {
		signal = 'S';
	} else {
		signal = 'L';
	}
	ss << signal;
	ss << time;
#if DEBUGINFO
	llvm::errs() << "createGlobalVarFullName : " << ss.str() << "\n";
#endif
	return ss.str();
}

unsigned Symbolic::getLoadTime(uint64_t address) {
	unsigned loadTime;
	std::map<uint64_t, unsigned>::iterator index = loadRecord.find(address);
	if (index == loadRecord.end()) {
		loadRecord.insert(std::make_pair(address, 1));
		loadTime = 1;
	} else {
		loadTime = index->second + 1;
		loadRecord[address] = loadTime;
	}
	return loadTime;
}

unsigned Symbolic::getStoreTime(uint64_t address) {
	unsigned storeTime;
	std::map<uint64_t, unsigned>::iterator index = storeRecord.find(address);
	if (index == storeRecord.end()) {
		storeRecord.insert(std::make_pair(address, 1));
		storeTime = 1;
	} else {
		storeTime = index->second + 1;
		storeRecord[address] = storeTime;
	}
	return storeTime;
}

void Symbolic::load(ExecutionState &state, KInstruction *ki) {
	std::string GlobalName;
	bool isGlobal;

	Type::TypeID id = ki->inst->getType()->getTypeID();
	ref<Expr> address = executor->eval(ki, 0, state).value;
	ObjectPair op;
#if DEBUGINFO
	ref<Expr> addressCurrent = executor->eval(ki, 0, state).value;
	llvm::errs() << "address : " << address << " address Current : "
			<< addressCurrent << "\n";
	bool successCurrent = executor->getMemoryObject(op, state,
			&state.addressSpace, addressCurrent);
	llvm::errs() << "successCurrent : " << successCurrent << "\n";
	llvm::errs() << "id : " << id << "\n";
#endif
	ConstantExpr* realAddress = dyn_cast<ConstantExpr>(address);
	if (realAddress) {
		uint64_t key = realAddress->getZExtValue();
		bool success = executor->getMemoryObject(op, state,
				&(state.addressSpace), address);
		if (success) {
			const MemoryObject *mo = op.first;
			isGlobal = executor->isGlobalMO(mo);
			if (isGlobal) {
				unsigned loadTime = getLoadTime(key);
				std::string ld;
				llvm::raw_string_ostream rso(ld);
				ki->inst->print(rso);
				std::stringstream ss;
				unsigned int j = rso.str().find("=");
				for (unsigned int i = 2; i < j; i++) {
					ss << rso.str().at(i);
				}
				if(mo->isGlobal){

				}else {
					ss << "con";
				}
				GlobalName = createGlobalVarFullName(ss.str(), mo->id, key,
						isGlobal, loadTime, false);
				if (id == Type::IntegerTyID) {
					Expr::Width size = executor->getWidthForLLVMType(
							ki->inst->getType());
					ref<Expr> value = executor->getDestCell(state, ki).value;
					ref<Expr> symbolic = manualMakeSymbolic(GlobalName, size);
#if DEBUGINFO
					std::cerr << " load symbolic value : ";
					symbolic->dump();
#endif
					executor->bindLocal(ki, state, symbolic);
					state.encode.globalname.push_back(GlobalName);
					state.encode.globalexpr.push_back(symbolic);
				}
			}
		} else {
			llvm::errs() << "Load address = " << realAddress->getZExtValue()
					<< "\n";
			std::string ld;
					llvm::raw_string_ostream rso(ld);
					ki->inst->print(rso);
					std::stringstream ss;
					unsigned int j = rso.str().find("=");
					for (unsigned int i = 2; i < j; i++) {
						ss << rso.str().at(i);
					}
					GlobalName = ss.str() + "erroraddr";
					if (id == Type::IntegerTyID || id == Type::PointerTyID) {
						Expr::Width size = executor->getWidthForLLVMType(
								ki->inst->getType());
						ref<Expr> symbolic = manualMakeSymbolic(GlobalName, size);
				executor->bindLocal(ki, state, symbolic);
						ref<Expr> value = executor->getDestCell(state, ki).value;
			#if DEBUGINFO
						std::cerr << " load symbolic value : ";
						symbolic->dump();
						std::cerr << " load value : ";
						value->dump();
			#endif
						state.encode.globalname.push_back(GlobalName);
						state.encode.globalexpr.push_back(symbolic);
					}else {
						assert(0 && " address is not const");
			}
		}
	} else {
		std::string ld;
		llvm::raw_string_ostream rso(ld);
		ki->inst->print(rso);
		std::stringstream ss;
		unsigned int j = rso.str().find("=");
		for (unsigned int i = 2; i < j; i++) {
			ss << rso.str().at(i);
		}
		GlobalName = ss.str() + "noaddr";
		if (id == Type::IntegerTyID || id == Type::PointerTyID) {
			Expr::Width size = executor->getWidthForLLVMType(
					ki->inst->getType());
			ref<Expr> symbolic = manualMakeSymbolic(GlobalName, size);
			executor->bindLocal(ki, state, symbolic);
			ref<Expr> value = executor->getDestCell(state, ki).value;
#if DEBUGINFO
			std::cerr << " load symbolic value : ";
			symbolic->dump();
			std::cerr << " load value : ";
			value->dump();
#endif
			state.encode.globalname.push_back(GlobalName);
			state.encode.globalexpr.push_back(symbolic);
		}else {
			assert(0 && " address is not const");
		}
	}
}

void Symbolic::callReturnValue(ExecutionState &state, KInstruction *ki,
		Function *function) {
	Type *resultType = ki->inst->getType();
	if (!resultType->isVoidTy()) {
		Expr::Width size = executor->getWidthForLLVMType(ki->inst->getType());
		std::string ld;
		llvm::raw_string_ostream rso(ld);
		ki->inst->print(rso);
		std::stringstream ss;
		unsigned int j = rso.str().find("=");
		for (unsigned int i = 2; i < j; i++) {
			ss << rso.str().at(i);
		}

		std::string GlobalName = ss.str() + "call return";
		ref<Expr> symbolic = manualMakeSymbolic(GlobalName, size);
		executor->bindLocal(ki, state, symbolic);
		state.encode.globalname.push_back(GlobalName);
		state.encode.globalexpr.push_back(symbolic);

		Instruction *i = ki->inst;
		llvm::CallSite cs(i);
		unsigned numArgs = cs.arg_size();
		for (unsigned j=0; j<numArgs; ++j) {
			std::string argName = ss.str() + "call arg" + std::to_string(j);
			Expr::Width size = executor->getWidthForLLVMType(cs.getArgument(j)->getType());
			ref<Expr> arg = manualMakeSymbolic(argName, size);
			executor->uneval(ki, j+1, state).value = arg;
#if DEBUGINFO
			std::cerr << "argName : " << argName << "\n";
			std::cerr << "size : " << size << "\n";
			std::cerr << "arg : ";
			arg->dump();
#endif
		}
	}
}

} /* namespace klee */
