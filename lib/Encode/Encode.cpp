/*
 * Encode.cpp
 *
 *  Created on: Oct 22, 2018
 *      Author: klee
 */

#include "Encode.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <iostream>

#include "json11.hpp"
#include "json.hpp"

using namespace llvm;
using namespace klee;

namespace klee {

cl::opt<std::string> WhiteList("white-list", cl::desc("The path of white list"),
		cl::init("./white-list"));

cl::opt<std::string> BlackList("black-list", cl::desc("The path of black list"),
		cl::init("./black-list"));
cl::opt<std::string> UseList("use-list", cl::desc("The path of use list"),
		cl::init("./use-list"));

cl::opt<std::string> jsonFile("json-file", cl::desc("The path of json file"),
		cl::init("./jsonfile"));

Encode::Encode() :
		z3_ctxx(new context()), z3_solverr(*z3_ctxx), kq(*z3_ctxx), flag(1) {
//	addList();
}

Encode::Encode(const Encode &e) :
		z3_ctxx(e.z3_ctxx), z3_solverr(e.z3_solverr), kq(*z3_ctxx), constraintExpr(
				e.constraintExpr), path(e.path), globalname(e.globalname), globalexpr(e.globalexpr), flag(e.flag), Json(e.Json), whiteList(
				e.whiteList), blackList(e.blackList), useList(e.useList), isWhiteList(
				e.isWhiteList) {
}

Encode::~Encode() {

}

void Encode::addList() {

	std::string err;
	const auto json = json11::Json::parse(Json, err);

#if DEBUG
	std::cerr << "whitelist" << "\n";
#endif

	std::string temp = json["use"].dump();
	temp.erase(temp.begin(), temp.begin() + 1);
	temp.erase(temp.end() - 1, temp.end());
	useList.push_back(temp);

	temp = json["whitelist"].dump();
	temp.erase(temp.begin(), temp.begin() + 1);
	temp.erase(temp.end() - 1, temp.end());
	unsigned int j = 0;
	for (unsigned int i = 0; i < temp.size(); i++) {
		if (temp.at(i) == ',') {
			std::string templist = temp.substr(j, i - j);
			templist.erase(templist.begin(), templist.begin() + 1);
			templist.erase(templist.end() - 1, templist.end());
			whiteList.push_back(templist);
#if DEBUG
			std::cerr << templist << "\n";
#endif
			j = i + 2;
		} else if (i == (temp.size() - 1)) {
			std::string templist = temp.substr(j, i - j);
			templist.erase(templist.begin(), templist.begin() + 1);
			whiteList.push_back(templist);
#if DEBUG
			std::cerr << templist << "\n";
#endif
		}
	}

#if DEBUG
	std::cerr << "blacklist" << "\n";
#endif

	temp = json["blacklist"].dump();
	temp.erase(temp.begin(), temp.begin() + 1);
	temp.erase(temp.end() - 1, temp.end());
	j = 0;
	for (unsigned int i = 0; i < temp.size(); i++) {
		if (temp.at(i) == ',') {
			std::string templist = temp.substr(j, i - j);
			templist.erase(templist.begin(), templist.begin() + 1);
			templist.erase(templist.end() - 1, templist.end());
			blackList.push_back(templist);
#if DEBUG
			std::cerr << templist << "\n";
#endif
			j = i + 2;
		} else if (i == (temp.size() - 1)) {
			std::string templist = temp.substr(j, i - j);
			templist.erase(templist.begin(), templist.begin() + 1);
			blackList.push_back(templist);
#if DEBUG
			std::cerr << templist << "\n";
#endif
		}
	}

//	std::ifstream whitelist(WhiteList);
//	if (whitelist.is_open()) {
//		while (getline(whitelist, line)) {
//			whiteList.push_back(line);
//#ifdef DEBUG
//			std::cerr << line << "\n";
//#endif
//		}
//		whitelist.close();
//	} else {
//		std::cerr << "Unable to open whitelist";
//	}
//
//#ifdef DEBUG
//	std::cerr << "blacklist" << "\n";
//#endif
//	std::ifstream blacklist(BlackList);
//	if (blacklist.is_open()) {
//		while (getline(blacklist, line)) {
//			blackList.push_back(line);
//#ifdef DEBUG
//			std::cerr << line << "\n";
//#endif
//		}
//		blacklist.close();
//	} else {
//		std::cerr << "Unable to open blacklist";
//	}
//
//#ifdef DEBUG
//	std::cerr << "uselist" << "\n";
//#endif
//	std::ifstream uselist(UseList);
//	if (uselist.is_open()) {
//		while (getline(uselist, line)) {
//			useList.push_back(line);
//#ifdef DEBUG
//			std::cerr << line << "\n";
//#endif
//		}
//		uselist.close();
//	} else {
//		std::cerr << "Unable to open uselist" << "\n";
//	}

	expr constraint = z3_ctxx->bool_val(1);
	for (unsigned i = 0; i < whiteList.size(); i++) {
		expr brLabel = z3_ctxx->int_const(whiteList[i].c_str());
		constraint = (brLabel == true);
		z3_solverr.add(constraint);
		isWhiteList.push_back(0);
	}
	isWhiteList[0] = 1;
#if DEBUG
	std::cerr << "whiteList : " << whiteList.size() << "\n";
#endif
}

void Encode::addBrConstraint(ref<Expr> cond, bool isTrue,
		llvm::StringRef labelTrue, llvm::StringRef labelFalse) {

	path.push_back(isTrue);

	expr constraint = z3_ctxx->bool_val(1);
	expr brCond = kq.getZ3Expr(cond);
	expr brIsTrue = z3_ctxx->bool_val(isTrue);
	constraint = (brCond == brIsTrue);
	constraintExpr.push_back(constraint);
#if DEBUG
	std::cerr << "addBr : " << constraint << "\n";
#endif
	expr brLabelTrue = z3_ctxx->int_const(labelTrue.str().c_str());
	constraint = (brLabelTrue == true);
	constraintExpr.push_back(constraint);
#if DEBUG
	std::cerr << "addBr : " << constraint << "\n";
#endif

//	expr brLabelFalse = z3_ctx->int_const(labelFalse.str().c_str());
//	constraint = (brLabelFalse == false);
//	constraintExpr.push_back(constraint);

#if DEBUG
	std::cerr << "all : \n";
	for (unsigned int i = 0; i < constraintExpr.size(); i++) {
		std::cerr << constraintExpr[i] << "\n";
	}
	std::cerr << "path : \n";
	for (unsigned int i = 0; i < path.size(); i++) {
		std::cerr << path[i] << "\n";
	}
#endif
}

void Encode::checkWhiteList(llvm::StringRef label) {
	for (unsigned i = 0; i < whiteList.size(); i++) {
#if DEBUG
		llvm::errs() << "whiteList : " << whiteList[i] << "\n";
#endif
		if (whiteList[i] == label.str()) {
			if (!isWhiteList[i]) {
				isWhiteList[i] = 1;
				flag++;
			}
		}
	}
#if DEBUG
	llvm::errs() << "label name : " << label << "\n";
	llvm::errs() << "flag : " << flag << "\n";
	std::cerr << "isWhiteList : " << isWhiteList.size() << "\n";
	for (unsigned int i = 0; i < isWhiteList.size(); i++) {
		std::cerr << "whiteList[" << i << "]" << ": " << isWhiteList[i] << "\n";
	}
#endif
}

void Encode::checkBlackList(llvm::StringRef label) {
	for (unsigned i = 0; i < blackList.size(); i++) {
		if (blackList[i] == label.str()) {
			flag = -1;
		}
	}
}

void Encode::checkUseList(llvm::StringRef label) {

	if (flag == whiteList.size()) {
		for (unsigned i = 0; i < useList.size(); i++) {
#if DEBUG
			llvm::errs() << "useList : " << useList[i] << "\n";
#endif
			if (useList[i] == label.str()) {
				for (unsigned int i = 0; i < constraintExpr.size(); i++) {
					z3_solverr.add(constraintExpr[i]);
				}
				check_result result = z3_solverr.check();
				if (result == z3::sat) {
					flag = 0;
					std::cerr << "Yes!\n";
					model m = z3_solverr.get_model();
					std::cerr << "\nz3_solver.get_model()\n";
					std::cerr << "\n" << m << "\n";

					auto json = nlohmann::json::parse(Json);
					json["find"] = "Yes";
					std::stringstream ss;
					ss.str("");
					for (unsigned int i = 0; i < path.size(); i++) {
						ss << path[i];
					}
					json["path"] = ss.str();

					for (unsigned int i = 0; i < globalexpr.size(); i++) {
						ss.str("");
						ss << m.eval(kq.getZ3Expr(globalexpr.at(i)));
						json["symbolic"][globalname.at(i)] = ss.str();
					}
					std::ofstream out_file("confirm_result.txt", std::ios_base::out | std::ios_base::app);
					out_file << json.dump() << "\n";
					out_file.close();
				} else if (result == z3::unknown) {

				} else if (result == z3::unsat) {
					std::cerr << "No!\n";
				}
			}
		}
	}
}

int Encode::checkList(llvm::StringRef label) {
	checkWhiteList(label);
	checkBlackList(label);
	checkUseList(label);
	return flag;
}

}
