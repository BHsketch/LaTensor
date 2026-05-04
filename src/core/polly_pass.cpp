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
#include "polly/LinkAllPasses.h"
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

    struct Stmt
    {
    };

    struct TreeNode { };

    struct LoopInfo : TreeNode
    {
        long lower_bound;
        long upper_bound;
        std::vector<std::shared_ptr<TreeNode>> children {};
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
    };

    // For iter var referencing loops i, j as 2i + 3j + 7 we have:
    struct IterVarInfo
    {
        // 2i, (+) 3j
        std::vector<AxisMultiplier> linear_combination {};
        // ... + 7
        int added_const;
        IterVarType type;

        // Default constructor, always sets linear combination to loop_var * 1 + 0
        IterVarInfo(std::shared_ptr<LoopInfo> loop_var)
        {
            this->linear_combination.emplace_back(AxisMultiplier(std::move(loop_var), 1));
            this->added_const = 0;
            this->type = spatial;
        }
    };

    struct BlockInfo : TreeNode
    {
        std::vector<IterVarInfo> iter_vars;
        std::vector<Stmt> stmts;

        explicit BlockInfo(polly::ScopStmt *Stmt)
        {

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

                for (const std::shared_ptr<LoopInfo>& li : loop_backtrace)
                {
                    IterVarInfo thing = IterVarInfo(li);
                    leaf->iter_vars.emplace_back(thing);
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
        static bool get_loop_bound(isl::ast_node &Node, const std::shared_ptr<LoopInfo>& tree_node)
        {
            long lower = 0;
            long upper = 0;

            // 1. EXTRACT THE LOWER BOUND (Init)
            isl::ast_expr init_expr = isl::manage(isl_ast_node_for_get_init(Node.get()));

            if (isl_ast_expr_get_type(init_expr.get()) == isl_ast_expr_int) {
                // It's a pure integer! Extract it.
                isl::val init_val = isl::manage(isl_ast_expr_get_val(init_expr.get()));
                lower = isl_val_get_num_si(init_val.get());
            } else {
                // It might be a variable or math expression (e.g., 'N' or 'N + 1')
                return false;
            }

            // 2. EXTRACT THE UPPER BOUND (Cond)
            // The condition is usually a binary operation like (c0 <= 127)
            isl::ast_expr cond_expr = isl::manage(isl_ast_node_for_get_cond(Node.get()));

            if (isl_ast_expr_get_type(cond_expr.get()) == isl_ast_expr_op) {
                // We expect an operator like '<' or '<='.
                // Argument 0 is the iterator (c0). Argument 1 is the limit (127).
                isl::ast_expr ub_expr = isl::manage(isl_ast_expr_get_op_arg(cond_expr.get(), 1));

                if (isl_ast_expr_get_type(ub_expr.get()) == isl_ast_expr_int) {
                    // The right hand side of the condition is a pure integer!
                    isl::val ub_val = isl::manage(isl_ast_expr_get_val(ub_expr.get()));
                    upper = isl_val_get_num_si(ub_val.get());

                    // Check if the operator was '<' or '<=' to know the exact range
                    enum isl_ast_op_type op_type = isl_ast_expr_get_op_type(cond_expr.get());
                    if (op_type == isl_ast_op_le) {
                        upper++;
                    } else if (op_type != isl_ast_op_lt) {
                        return false;
                    }
                } else {
                    return false;
                }
            }

            tree_node->lower_bound = lower;
            tree_node->upper_bound = upper;
            return true;
        }
    };
} // end anonymous namespace

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