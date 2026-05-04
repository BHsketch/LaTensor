#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h" // Added for PreservedAnalyses & PassInfoMixin
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "polly/RegisterPasses.h"

// Polly headers
#include "polly/ScopInfo.h"
#include <isl/ast.h>

#include <utility>
#include "polly/DependenceInfo.h"  // For DependenceInfo, DependenceAnalysis, TYPE_RAW
#include "polly/CodeGen/IslAst.h"          // For IslAstInfo, IslAstAnalysis
#include "llvm/Analysis/RegionInfo.h"

using namespace llvm;

namespace latensor
{

    enum IterVarType
    {
        spatial,
        reduction,
        scan
    };

    struct TreeNode
    {
        virtual std::string to_string() = 0;
    };

    std::string indent_lines(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 4 * 10);

        out += "    ";
        for (int i = 0; i < input.length(); i++)
        {
            char c = input[i];
            out += c;
            if (c == '\n' && i < input.length() - 1)
            {
                out += "    ";
            }
        }
        return out;
    }

    struct LoopInfo : TreeNode
    {
        std::string name;
        long lower_bound;
        long upper_bound;
        std::vector<std::shared_ptr<TreeNode>> children {};

        std::string to_string()
        {
            std::string s = "for " + name + " in T.serial(" + std::to_string(lower_bound) + ", " + std::to_string(upper_bound - lower_bound) + "):\n";
            for (const auto& child : children)
            {
                s += indent_lines(child->to_string());
            }
            return s;
        }
    };

    // for example, 2i
    struct AxisMultiplier
    {
        // i
        std::shared_ptr<LoopInfo> loop_var;
        // 2
        int multiplier;

        AxisMultiplier(std::shared_ptr<LoopInfo> loop_var, int mul)
        {
            this->loop_var = std::move(loop_var);
            this->multiplier = mul;
        }

        std::string to_string()
        {
            return std::to_string(multiplier) + " * " + loop_var->name;
        }
    };

    // For iter var referencing loops i, j as 2i + 3j + 7 we have:
    struct IterVarInfo
    {
        // 2i, (+) 3j
        std::vector<AxisMultiplier> linear_combination {};
        // ... + 7
        int added_const;
        IterVarType type;

        long extent;
        std::string name;

        // Default constructor, always sets linear combination to loop_var * 1 + 0
        IterVarInfo(std::shared_ptr<LoopInfo> loop_var, int num)
        {
            this->extent = loop_var->upper_bound - loop_var->lower_bound;
            this->name = "vi" + std::to_string(num);
            this->linear_combination.emplace_back(AxisMultiplier(std::move(loop_var), 1));
            this->added_const = 0;
            this->type = spatial;
        }

        std::string to_string()
        {
            std::string t;
            switch (this->type)
            {
                case spatial:
                    t = "spatial";
                    break;
                case reduction:
                    t = "reduce";
                    break;
                case scan:
                    t = "scan";
                    break;
            }

            std::string ac = std::to_string(this->added_const);
            for (AxisMultiplier &am: linear_combination)
            {
                ac += " + " + am.to_string();
            }

            return name + " = T.axis." + t + "(" + std::to_string(extent) + ", " + ac + ")";
        }
    };

    struct MemoryAccessInfo
    {
        std::string array_name;
        std::string access_str; // e.g., "128*i0 + i1"
        bool is_read;
        bool is_write;
    };

    struct TVMComputeStmt
    {
        std::string lhs; // e.g., "C[vi, vj]"
        std::string rhs; // e.g., "A[vi, vk] * B[vk, vj]"
    };

    std::string buildMathematicalAccess(polly::MemoryAccess *MA) {
        // 1. Get the Array Name (e.g., "MemRef1")
        std::string array_name = MA->getScopArrayInfo()->getName();

        // 2. Get the access relation and convert it to a piecewise multi-affine expression
        isl::map access_map = MA->getAccessRelation();
        isl::pw_multi_aff pma = isl::pw_multi_aff::from_map(access_map);

        // Release to the C API for deep inspection
        isl_pw_multi_aff *pma_c = pma.release();
        std::string index_expr = "";

        // An access might theoretically have multiple piecewise conditions (e.g., if/else inside loop)
        // For standard loops, there is only one piece. We extract it using a callback:
        isl_multi_aff *ma = nullptr;
        isl_pw_multi_aff_foreach_piece(pma_c, [](isl_set *set, isl_multi_aff *m_aff, void *user) -> isl_stat {
            isl_multi_aff **ma_ptr = static_cast<isl_multi_aff **>(user);
            if (!*ma_ptr) {
                *ma_ptr = isl_multi_aff_copy(m_aff); // Copy the first math piece we find
            }
            isl_set_free(set);
            isl_multi_aff_free(m_aff);
            return isl_stat_ok;
        }, &ma);

        if (ma) {
            // We want the math for the 1st dimension of the array access (Index 0)
            // E.g., for MemRef1[128 * i0 + i1], this gets the (128 * i0 + i1) expression
            isl_aff *aff = isl_multi_aff_get_aff(ma, 0);

            // How many loop iterators (i0, i1, i2) does this math use?
            int num_in_dims = isl_aff_dim(aff, isl_dim_in);

            bool is_first = true;

            // 3. EXTRACT THE COEFFICIENTS (e.g., the '128' in 128*vi0)
            for (int i = 0; i < num_in_dims; ++i) {
                // Get the integer multiplier for dimension 'i'
                isl_val *coef_val = isl_aff_get_coefficient_val(aff, isl_dim_in, i);
                long coef = isl_val_get_num_si(coef_val);
                isl_val_free(coef_val);

                if (coef != 0) {
                    if (!is_first) index_expr += " + ";

                    if (coef == 1) {
                        index_expr += "vi" + std::to_string(i);
                    } else {
                        index_expr += std::to_string(coef) + " * vi" + std::to_string(i);
                    }
                    is_first = false;

                    // NOTE: If you are populating your IterVarInfo struct,
                    // you would do `my_iter_info.linear_combination.push_back({vi_ptr, coef});` here!
                }
            }

            // 4. EXTRACT THE CONSTANT (e.g., the '7' in vi0 + 7)
            isl_val *const_val = isl_aff_get_constant_val(aff);
            long const_term = isl_val_get_num_si(const_val);
            isl_val_free(const_val);

            if (const_term != 0) {
                if (!is_first) index_expr += " + ";
                index_expr += std::to_string(const_term);

                // NOTE: `my_iter_info.added_const = const_term;`
            }

            if (index_expr.empty()) index_expr = "0";

            isl_aff_free(aff);
            isl_multi_aff_free(ma);
        }

        isl_pw_multi_aff_free(pma_c);

        return array_name + "[" + index_expr + "]";
    }


    // Helper to recursively reconstruct the math expression
    std::string buildComputeMath(llvm::Value *Val, polly::ScopStmt *Stmt)
    {
        // 1. Is it a Load? (Base case)
        if (auto *Load = llvm::dyn_cast<llvm::LoadInst>(Val))
        {
            // Find which Polly MemoryAccess corresponds to this load
            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->getAccessInstruction() == Load)
                    return buildMathematicalAccess(MA); // Replace with actual index
            }
            return "UnknownLoad";
        }

        // 2. Is it a mathematical operation? (Recursive case)
        if (auto *BinOp = llvm::dyn_cast<llvm::BinaryOperator>(Val))
        {
            std::string op = "";
            switch (BinOp->getOpcode())
            {
                case llvm::Instruction::FAdd:
                    op = " + ";
                    break;
                case llvm::Instruction::FMul:
                    op = " * ";
                    break;
                    // todo add other operations as needed...
            }

            std::string left = buildComputeMath(BinOp->getOperand(0), Stmt);
            std::string right = buildComputeMath(BinOp->getOperand(1), Stmt);
            return "(" + left + op + right + ")";
        }

        // 3. Is it a Constant?
        if (auto *ConstFP = llvm::dyn_cast<llvm::ConstantFP>(Val))
            return std::to_string(ConstFP->getValueAPF().convertToFloat());

        return "UnsupportedValue";
    }

    struct BlockInfo : TreeNode
    {
        std::string name;
        std::vector<IterVarInfo> iter_vars;
        std::vector<MemoryAccessInfo> reads;
        std::vector<MemoryAccessInfo> writes;
        std::vector<TVMComputeStmt> compute_stmts;

        explicit BlockInfo(polly::ScopStmt *Stmt)
        {
            this->name = Stmt->getBaseName();
            for (polly::MemoryAccess *MA: *Stmt)
            {
                // Skip scalar/register dependencies for TVM buffer mappings
                if (!MA->isArrayKind()) continue;

                MemoryAccessInfo mem_info;
                mem_info.array_name = MA->getScopArrayInfo()->getName();
                mem_info.is_read = MA->isRead();
                mem_info.is_write = MA->isWrite();

                // Get the mathematical index mapping (e.g., { Stmt[i,j] -> MemRef_A[128i + j] })
                isl::map access_map = MA->getAccessRelation();

                isl::pw_multi_aff pma = isl::pw_multi_aff::from_map(access_map);
                mem_info.access_str = buildMathematicalAccess(MA);

                if (mem_info.is_read)
                    this->reads.push_back(mem_info);
                if (mem_info.is_write)
                    this->writes.push_back(mem_info);
            }

            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->isWrite() && MA->isArrayKind())
                {
                    llvm::Instruction *StoreI = MA->getAccessInstruction();
                    if (auto *StoreOp = llvm::dyn_cast<llvm::StoreInst>(StoreI))
                    {
                        TVMComputeStmt compute;

                        // LHS: The array we are writing to
                        compute.lhs = buildMathematicalAccess(MA);

                        // RHS: Recursively build the math expression starting from the value being stored
                        llvm::Value *RHS_Value = StoreOp->getValueOperand();
                        compute.rhs = buildComputeMath(RHS_Value, Stmt);

                        this->compute_stmts.push_back(compute);

                        llvm::errs() << "Extracted Compute: " << compute.lhs << " = " << compute.rhs << "\n";
                    }
                }
            }
        }

        std::string to_string()
        {
            std::string s = "with T.block(\"" + this->name + "\"):\n";
            for (auto iv: iter_vars)
            {
                s += "    " + iv.to_string() + "\n";
            }
            s += "    T.reads(";
            for (int i = 0; i < this->reads.size(); i++)
            {
                if (i > 0)
                    s += ", ";
                s += this->reads[i].access_str;
            }
            s += ")\n    T.writes(";
            for (int i = 0; i < this->writes.size(); i++)
            {
                if (i > 0)
                    s += ", ";
                s += this->writes[i].access_str;
            }
            s += ")\n";
            for (const auto& st: this->compute_stmts)
            {
                s += "    " + st.lhs + " = " + st.rhs + "\n";
            }
            return s;
        }
    };

    // 1. Inherit from PassInfoMixin instead of FunctionPass
    struct MyPollyScopPass : public PassInfoMixin<MyPollyScopPass>
    {
        // 2. Use the NPM run signature
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM)
        {
            // 3. Get ScopInfo using the New Pass Manager Analysis Manager
            polly::ScopInfo &SI = FAM.getResult<polly::ScopInfoAnalysis>(F);

            polly::ScopAnalysisManager &SAM = FAM.getResult<polly::ScopAnalysisManagerFunctionProxy>(F).getManager();

            if (SI.empty())
            {
                errs() << "No SCoPs in function: " << F.getName() << "\n";
                return PreservedAnalyses::all();
            }

            errs() << "Function: " << F.getName() << "\n";

            polly::ScopStandardAnalysisResults AR = {
                    FAM.getResult<llvm::DominatorTreeAnalysis>(F),
                    SI,
                    FAM.getResult<llvm::ScalarEvolutionAnalysis>(F),
                    FAM.getResult<llvm::LoopAnalysis>(F),
                    FAM.getResult<llvm::RegionInfoAnalysis>(F),
                    FAM.getResult<llvm::TargetIRAnalysis>(F)
            };

            for (auto &Entry: SI)
            {
                polly::Scop *S = Entry.second.get();
                errs() << "  SCoP: " << S->getNameStr() << "\n";


                // 1. Get the ISL AST Analysis for this SCoP
                auto &ASTInfo = SAM.getResult<polly::IslAstAnalysis>(*S, AR);
                isl::ast_node RootNode = ASTInfo.getAst();

                // 2. Build the entire tree of LoopInfos and BlockInfos!
                std::vector<std::shared_ptr<TreeNode>> LoopTree = walkAST(RootNode, {});
                errs() << "TVM CODE IS:\n" << LoopTree[0]->to_string() + "";

                // Now LoopTree contains your fully structured, nested hierarchy.

                // 1. Let auto resolve the exact wrapper type returned by the Pass Manager
                auto &DI = SAM.getResult<polly::DependenceAnalysis>(*S, AR);

                // 2. Get the Dependences analysis object at the statement level
                const polly::Dependences &Deps = DI.getDependences(polly::Dependences::AL_Statement);

                // 3. Extract the actual ISL union_map for the True (RAW) dependencies
                isl::union_map TrueDeps = Deps.getDependences(polly::Dependences::TYPE_RAW);

                std::map<polly::ScopStmt *, IterVarType> stmt_types{};

                for (polly::ScopStmt &Stmt: *S)
                {
                    IterVarType current_stmt_type = spatial;

                    errs() << "    Stmt: " << Stmt.getBaseName() << "\n";

                    isl::set Domain = Stmt.getDomain();
                    char *DomainStr = isl_set_to_str(Domain.get());
                    errs() << "      Loop Bounds (Domain): " << DomainStr << "\n";
                    free(DomainStr); // You must free the C-string!

                    for (Instruction *I: Stmt.getInstructions())
                    {
                        errs() << "      Inst: " << *I << "\n";
                    }

                    for (polly::MemoryAccess *MA: Stmt)
                    {
                        errs() << "      MemAccess: ";
                        MA->print(errs());
                        errs() << "\n";

                        if (MA->isReductionLike())
                        {
                            current_stmt_type = reduction;
                        }

                        if (MA->isWrite())
                        {
                            Instruction *StoreI = MA->getAccessInstruction();
                            errs() << "        Write: " << *StoreI << "\n";

                            if (auto *StoreOp = dyn_cast<StoreInst>(StoreI))
                            {
                                Value *RHS = StoreOp->getValueOperand();
                                errs() << "        RHS: " << *RHS << "\n";
                            }
                        }
                    }
                }

                S->print(llvm::errs(), false);
            }

            // 4. Return the analyses preserved (all of them, since we only read)
            return PreservedAnalyses::all();
        }

        // The main recursive walker
        std::vector<std::shared_ptr<TreeNode>> walkAST(isl::ast_node Node, std::vector<std::shared_ptr<LoopInfo>> loop_backtrace)
        {
            std::vector<std::shared_ptr<TreeNode>> result;

            if (Node.is_null())
                return result;

            // 1. IS THIS A FOR LOOP?
            if (isl_ast_node_get_type(Node.get()) == isl_ast_node_for)
            {
                auto current_loop = std::make_shared<LoopInfo>();
                bool found = get_loop_bound(Node, current_loop);
                if (!found)
                    throw std::runtime_error("LaTensor: Failed to convert -- loop bounds are not simple");

                errs() << "loop begin\n";
                // Recurse into the body of the loop

                errs() << isl_ast_node_to_str(Node.get()) << "\n";
                isl::ast_node Body = isl::manage(isl_ast_node_for_get_body(Node.get()));
                loop_backtrace.emplace_back(current_loop);
                current_loop->children = walkAST(Body, loop_backtrace);

                errs() << "loop end\n";

                result.push_back(current_loop);
            }

                // 2. IS THIS A BLOCK (SIDE-BY-SIDE SIBLINGS)?
            else if (isl_ast_node_get_type(Node.get()) == isl_ast_node_block)
            {
                isl::ast_node_list List = isl::manage(isl_ast_node_block_get_children(Node.get()));
                int num_children = isl_ast_node_list_n_ast_node(List.get());

                // Iterate through all siblings in this block
                for (int i = 0; i < num_children; i++)
                {
                    isl::ast_node ChildNode = isl::manage(isl_ast_node_list_get_ast_node(List.get(), i));

                    errs() << "siblings begin\n";
                    // Recurse and append all resulting loops to our current level
                    auto siblings = walkAST(ChildNode, loop_backtrace);
                    result.insert(result.end(), siblings.begin(), siblings.end());
                    errs() << "siblings end\n";

                }
            }

            // 3. IS THIS A COMPUTATION NODE (SCOPSTMT)?
            else if (isl_ast_node_get_type(Node.get()) == isl_ast_node_user)
            {
                // --- THE POLLY MAGIC TRICK ---
                // Get the mathematical expression inside the node
                isl::ast_expr Expr = isl::manage(isl_ast_node_user_get_expr(Node.get()));

                // The first argument of the expression contains the ID
                isl::ast_expr Arg = isl::manage(isl_ast_expr_get_op_arg(Expr.get(), 0));
                isl::id Id = isl::manage(isl_ast_expr_get_id(Arg.get()));

                // The user pointer of this ID is exactly our ScopStmt!
                polly::ScopStmt *Stmt = static_cast<polly::ScopStmt *>(isl_id_get_user(Id.get()));

                // Create a leaf node in your tree to hold this Block
                auto leaf = std::make_shared<BlockInfo>(Stmt);
                errs() << "a block!\n";
                errs() << *Stmt << "\n";

                int i = 0;
                for (const std::shared_ptr<LoopInfo> &li: loop_backtrace)
                {
                    IterVarInfo thing = IterVarInfo(li, i);
                    leaf->iter_vars.emplace_back(thing);
                    i++;
                }

                result.push_back(leaf);
            }

                // 4. IS THIS A MARK NODE (METADATA)?
            else if (isl_ast_node_get_type(Node.get()) == isl_ast_node_mark)
            {
                // Optional: You can extract the mark's name to see what optimization
                // Polly is suggesting (e.g., vectorization/parallelization)
                isl::id MarkId = isl::manage(isl_ast_node_mark_get_id(Node.get()));
                std::string mark_name = isl_id_get_name(MarkId.get());
                // llvm::errs() << "Found a mark: " << mark_name << "\n";

                // Peel off the mark wrapper to get the actual AST node inside
                isl::ast_node InnerNode = isl::manage(isl_ast_node_mark_get_node(Node.get()));

                // Recurse directly into the inner node and return its result!
                return walkAST(InnerNode, loop_backtrace);
            }

            return result;
        }

        // Returns true if ast node has nice lower/upper bounds, and populates the treeNode values.
        static bool get_loop_bound(isl::ast_node &Node, const std::shared_ptr<LoopInfo> &tree_node)
        {
            long lower = 0;
            long upper = 0;

            // 1. EXTRACT THE LOWER BOUND (Init)
            isl::ast_expr init_expr = isl::manage(isl_ast_node_for_get_init(Node.get()));

            if (isl_ast_expr_get_type(init_expr.get()) == isl_ast_expr_int)
            {
                // It's a pure integer! Extract it.
                isl::val init_val = isl::manage(isl_ast_expr_get_val(init_expr.get()));
                lower = isl_val_get_num_si(init_val.get());
            }
            else
            {
                // It might be a variable or math expression (e.g., 'N' or 'N + 1')
                return false;
            }

            // 2. EXTRACT THE UPPER BOUND (Cond)
            // The condition is usually a binary operation like (c0 <= 127)
            isl::ast_expr cond_expr = isl::manage(isl_ast_node_for_get_cond(Node.get()));

            if (isl_ast_expr_get_type(cond_expr.get()) == isl_ast_expr_op)
            {
                // We expect an operator like '<' or '<='.
                // Argument 0 is the iterator (c0). Argument 1 is the limit (127).
                isl::ast_expr ub_expr = isl::manage(isl_ast_expr_get_op_arg(cond_expr.get(), 1));

                if (isl_ast_expr_get_type(ub_expr.get()) == isl_ast_expr_int)
                {
                    // The right hand side of the condition is a pure integer!
                    isl::val ub_val = isl::manage(isl_ast_expr_get_val(ub_expr.get()));
                    upper = isl_val_get_num_si(ub_val.get());

                    // Check if the operator was '<' or '<=' to know the exact range
                    enum isl_ast_op_type op_type = isl_ast_expr_get_op_type(cond_expr.get());
                    if (op_type == isl_ast_op_le)
                        upper++;
                    else if (op_type != isl_ast_op_lt)
                        return false;
                }
                else
                    return false;
            }

            // 3. EXTRACT THE LOOP VARIABLE NAME (Iterator)
            // Get the iterator expression from the for loop
            isl::ast_expr iter_expr = isl::manage(isl_ast_node_for_get_iterator(Node.get()));

            // Extract the ID object from the expression
            isl::id iter_id = isl::manage(isl_ast_expr_get_id(iter_expr.get()));

            // Extract the actual C string name from the ID
            std::string iter_name = isl_id_get_name(iter_id.get());
            tree_node->name = iter_name;

            tree_node->lower_bound = lower;
            tree_node->upper_bound = upper;
            return true;
        }
    };
} // end namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
    return {
            LLVM_PLUGIN_API_VERSION, "MyPollyScopPass", "0.1",
            [](llvm::PassBuilder &PB)
            {

                // THE MAGIC LINE: Manually register Polly's analyses with the PassBuilder
                polly::registerPollyPasses(PB);

                // Keep your existing explicit parsing setup
                PB.registerPipelineParsingCallback(
                        [](llvm::StringRef Name,
                           llvm::FunctionPassManager &FPM,
                           llvm::ArrayRef<llvm::PassBuilder::PipelineElement>)
                        {
                            if (Name == "my-polly-scop-pass")
                            {
                                FPM.addPass(latensor::MyPollyScopPass());
                                return true;
                            }
                            return false;
                        });
            }
    };
}

/*
Algorithm:
root = build_isl_ast()
process_ast_node(root)

void process_ast_node(){
  - instantiate LoopInfo
  - get domain for node
  - get schedule for node
  - fill LoopInfo
  for each child of node {
    process_ast_node(child)  
  }
}

 for (i1) {
    for(i2) {
    }
    for(i3) {
     stmt (i1, i3)
    }
 }

 for node in post order traversal of tree:
    map.update(node.

*/