/**
 * Obfuscation pass which replace constant value 0 by x ^ x
 * where x is a visible integer register
 *
 * Development of an llvm pass
 *
 */

#define DEBUG_TYPE "XorZero"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
STATISTIC(XorCOUNT, "The # of substituted instructions");

#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <random>
#include <vector>

#include "Utils.h"

static llvm::cl::opt<Ratio> XoRatio{
  "XorZero-ratio",
    llvm::cl::desc("Only apply the XorZero pass on <ratio> of the candidates"),
    llvm::cl::value_desc("ratio"), llvm::cl::init(1.), llvm::cl::Optional};

using namespace llvm;

namespace {
  class	XorZero : public BasicBlockPass {

  public:
    // The address of this static is used to uniquely
    // identify this pass in the pass registry.
    // The PassManager relies on this address to find instance of
    // analyses pass and build dependencies on demand.
    static char				ID;
    compat::RandomNumberGenerator	RNG;

    XorZero()
      : BasicBlockPass(ID), RNG(nullptr)
    {}

    bool	doInitialization(Module &M) override {
      RNG = M.createRNG(this);
      return false;
    }

    bool	runOnBasicBlock(BasicBlock &block) override {
      registerVec.clear(); /* clear the vector of saved register*/
      bool	modified = false; /* default return value */
      std::uniform_real_distribution<double>	Dist(0., 1.);

      // Iterate on each block
      for (BasicBlock::iterator it = block.getFirstInsertionPt(),
	     ie = block.end(); it != block.end(); ++it) {
	Instruction &inst = *it;

	// Iterate on each operand of a block
	for (size_t i = 0; i < inst.getNumOperands(); ++i) {

	  if (Dist(RNG) > XoRatio.getRatio())
	    continue;
	  // Obfuscation of zero integer
	  if (Constant *c = isZeroInteger(inst.getOperand(i))) {
	    if (Value *new_val = replaceZeroInteger(inst, c)) {
	      inst.setOperand(i, new_val);
	      modified = true;
	      ++XorCOUNT;
	    }
	  }

	  // Obfuscation of zero in
	  // Store, Ret and Call instructions
	  if (isValidCandidateInst(inst)) {
	    if (isValidCandidateOperand(inst.getOperand(i))) {
	      Value *new_value = replaceZero(inst, i);
	      if (nullptr != new_value) {
		ReplaceInstWithValue(block.getInstList(), it, new_value);
		--it;
	      }
	      modified = true;
	      ++XorCOUNT;
	    }
	  }
	}
	// Register zero integer value
	registerValue(&inst);
      }
      return modified;
    }

  private:
    std::vector<Value*>	registerVec;

    // Register zero integer value
    // in registerVec during iteration
    // on a each instruction of a BasicBlock
    void	registerValue(Value *v) {
      if (v->getType()->isIntegerTy()) {
	this->registerVec.push_back(v);
      }
    }

    // Call a "replace" function according to
    // the opcode of the current instruction
    Value	*replaceZero(Instruction &inst, int operand_num) {
      IRBuilder<>	Builder(&inst);
      unsigned int	Opcode = inst.getOpcode();

      if (Instruction::Store == Opcode) {
	return replaceZeroStore(Builder, inst, operand_num);
      } else if (Instruction::Ret == Opcode) {
	return replaceZeroRet(Builder, inst, operand_num);
      } else if (Instruction::Call == Opcode) {
      	replaceZeroCall(Builder, inst, operand_num);
      }

      return nullptr;
    }

    // Obfuscate zero integer
    // replace 0 by x ^ x
    Value	*replaceZeroInteger(Instruction &inst, Value *vreplace) {
      if (this->registerVec.empty())
	return vreplace;

      // Generate a random index
      IRBuilder<>		Builder(&inst);
      std::random_device	rd;
      std::mt19937		gen(rd());
      std::uniform_int_distribution<size_t> Rand(0,
						 registerVec.size() - 1);
      size_t	idx = Rand(gen);
      Value	*new_value = Builder.CreateXor(registerVec.at(idx),
					       registerVec.at(idx));

      return new_value;
    }

    // Obfuscate a Store instruction
    // Replace int i = 0 by
    // x = i
    // y = x ^ x
    // i = y
    Value	*replaceZeroStore(IRBuilder<> &Builder,
				  Instruction &inst,
				  int operand_num) {
      Value	*op_value = inst.getOperand(1);

      return createXor(inst, op_value);
    }

    // Obfuscate a Return instruction
    // Replace return 0 by
    // %0 = alloca i32
    // x = %0
    // y = x ^ x
    // %0 = y
    // value = %0
    // return value;
    Value	*replaceZeroRet(IRBuilder<> &Builder,
				Instruction &inst,
				int operand_num) {
      Value	*n_val;

      n_val = Builder.CreateAlloca(inst.getOperand(operand_num)->getType());
      createXor(inst, n_val);
      return Builder.CreateRet(Builder.CreateLoad(n_val));
    }

    // Obfuscate a Call instruction
    // int	foo(0) {return 1;}
    // obfuscate every zero operand of foo function
    void	replaceZeroCall(IRBuilder<> &Builder,
				Instruction &inst,
				int operand_num) {
      Value	*n_val = nullptr;
      n_val = Builder.CreateAlloca(inst.getOperand(operand_num)->getType());

      createXor(inst, n_val);
      if (CallInst *ci = dyn_cast<CallInst>(&inst)) {
	Function *fct = ci->getCalledFunction();
	ci->setArgOperand(operand_num, Builder.CreateLoad(n_val));
      }
    }

    Value	*createXor(Instruction &inst,
			   Value *vreplace) {
      IRBuilder<>	Builder(&inst); // Builder

      // Create a load instruction to be read from memory
      // Create a xor operation and create a store instruction
      // to write the result of x ^ x at vreplace's address
      Value *val_load = Builder.CreateLoad(vreplace);
      Value *new_value = Builder.CreateStore(Builder.CreateXor(val_load,
							       val_load),
					     vreplace);

      return new_value;
    }

    // Check if the value v is a zero integer operand
    Constant*	isZeroInteger(Value *v) {
      Constant* c = nullptr;

      if (nullptr == (c = dyn_cast<Constant>(v))) /* is c is a constant? */
	return nullptr;
      if (false == (c->isNullValue())) /* is c is a null value? */
	return nullptr;
      if (false == c->getType()->isIntegerTy()) /* is c is an integer? */
	return nullptr;
      return c; /* return constant c */
    }

    // Check if instruction inst is
    // - Store instruction
    // - Return instruction
    // - Call instruction
    bool	isValidCandidateInst(Instruction &inst) {
      if (isa<StoreInst>(&inst)) { /* is Store? */
	return true;
      } else if (isa<ReturnInst>(&inst)) { /* is Return? */
	return true;
      } else if (isa<CallInst>(&inst)) { /* is Call? */
	return true;
      }
      return false;
    }

    // Check if value v is a ConstantInt
    // check if ConstantInt c  is a zero value
    bool	isValidCandidateOperand(Value *v) {
      if (ConstantInt* c = dyn_cast<ConstantInt>(v)) {
	if (c->isZero()) {
	  return true;
	}
      }
      return false;
    }
  };
}

char	XorZero::ID = 0;

static	RegisterPass<XorZero> X("XorZero", // pass option
				"Obfuscate with zero", // pass description
				true, // We don't modify the CFG
				false); // We are not writting an analysis

static void
registerClangPass(const PassManagerBuilder &,
		  legacy::PassManagerBase &PM) {
  PM.add(new XorZero());
}

static RegisterStandardPasses
RegisterXorZeroPass(PassManagerBuilder::EP_EarlyAsPossible,
		    registerClangPass);
