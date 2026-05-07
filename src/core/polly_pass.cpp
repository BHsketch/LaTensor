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
                {
                    return buildMathematicalAccess(MA); // Replace with actual index
                }
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
                    // add other operations as needed...
            }

            std::string left = buildComputeMath(BinOp->getOperand(0), Stmt);
            std::string right = buildComputeMath(BinOp->getOperand(1), Stmt);
            return "(" + left + op + right + ")";
        }

        // 3. Is it a Constant?
        if (auto *ConstFP = llvm::dyn_cast<llvm::ConstantFP>(Val))
        {
            return std::to_string(ConstFP->getValueAPF().convertToFloat());
        }

        return "UnsupportedValue";
    }

    struct BlockInfo : TreeNode
    {
        std::vector<IterVarInfo> iter_vars;
        std::vector<Stmt> stmts;
        std::vector<MemoryAccessInfo> mem_accesses;
        std::vector<TVMComputeStmt> compute_stmts;

        explicit BlockInfo(polly::ScopStmt *Stmt)
        {
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

                // To cleanly translate this to TVM, you extract the output dimension math.
                // A quick hack for prototyping is converting it to a C string:
                // In production, you'd use isl_pw_multi_aff to map this directly to TVM TIR nodes.
                isl::pw_multi_aff pma = isl::pw_multi_aff::from_map(access_map);
                mem_info.access_str = buildMathematicalAccess(MA);
                errs() << "aaaaaaaaaa " << mem_info.array_name << " " << mem_info.is_read << " "
                       << mem_info.is_write << " " << mem_info.access_str << "\n";

                this->mem_accesses.push_back(mem_info);
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
    };

    // ========================================================================
    // Reduction / scan classification (Allen-Kennedy properties)
    //
    // Property 1 (reduction structure): the stmt has self-RAW + self-WAR +
    //                                   self-WAW dependences.
    // Property 2 (no leakage):           no outgoing RAW from this stmt to any
    //                                   *other* stmt. Violation => scan.
    // Property 3 (clean accumulator):    the RHS of the single write reads the
    //                                   accumulator location exactly once.
    //
    // Classification:
    //   prop1 false                       -> spatial
    //   prop1 true,  prop3 false          -> ERROR (complicated reduction)
    //   prop1 true,  prop3 true,  prop2 ok    -> reduction
    //   prop1 true,  prop3 true,  prop2 violated -> scan
    //
    // Single-write constraint: stmts with multiple array writes are not
    // analyzed; they fall back to spatial with a warning. Under this
    // constraint AL_Statement deps are sufficient (the only write target IS
    // the accumulator).
    // ========================================================================

    struct ClassifyResult
    {
        IterVarType stmt_type;          // spatial | reduction | scan
        std::vector<unsigned> red_axes; // domain dim indices that carry the reduction/scan dependence
    };

    // Filter a union_map down to the (single) map whose source AND sink tuples
    // both equal `name`. Returns null isl::map if no such map exists.
    static isl::map extractSelfMap(const isl::union_map &UM, const std::string &name)
    {
        struct Ctx { std::string name; isl_map *result; };
        Ctx ctx{name, nullptr};
        isl_union_map_foreach_map(UM.get(),
            [](isl_map *m, void *user) -> isl_stat {
                auto *c = static_cast<Ctx *>(user);
                const char *src = isl_map_get_tuple_name(m, isl_dim_in);
                const char *snk = isl_map_get_tuple_name(m, isl_dim_out);
                if (src && snk && c->name == src && c->name == snk) {
                    if (c->result) isl_map_free(c->result);
                    c->result = m;
                } else {
                    isl_map_free(m);
                }
                return isl_stat_ok;
            }, &ctx);
        return ctx.result ? isl::manage(ctx.result) : isl::map();
    }

    // True iff the union_map contains any map whose source tuple equals `name`
    // and sink tuple does NOT equal `name` -- i.e., this stmt's accumulator is
    // read by some other stmt.
    static bool hasOutgoingDepToOther(const isl::union_map &UM, const std::string &name)
    {
        struct Ctx { std::string name; bool found; };
        Ctx ctx{name, false};
        isl_union_map_foreach_map(UM.get(),
            [](isl_map *m, void *user) -> isl_stat {
                auto *c = static_cast<Ctx *>(user);
                const char *src = isl_map_get_tuple_name(m, isl_dim_in);
                const char *snk = isl_map_get_tuple_name(m, isl_dim_out);
                if (src && snk && c->name == src && c->name != snk)
                    c->found = true;
                isl_map_free(m);
                return isl_stat_ok;
            }, &ctx);
        return ctx.found;
    }

    // Walk Val's expression tree; count LoadInsts whose Polly MemoryAccess has
    // an access relation equal to WriteMA's (i.e., reads the accumulator cell).
    static int countAccumLoads(llvm::Value *Val,
                               polly::ScopStmt *Stmt,
                               polly::MemoryAccess *WriteMA)
    {
        if (!Val) return 0;
        if (auto *Load = llvm::dyn_cast<llvm::LoadInst>(Val)) {
            for (polly::MemoryAccess *MA : *Stmt) {
                if (MA->getAccessInstruction() == Load) {
                    return MA->getAccessRelation().is_equal(WriteMA->getAccessRelation()) ? 1 : 0;
                }
            }
            return 0;
        }
        if (auto *I = llvm::dyn_cast<llvm::Instruction>(Val)) {
            int sum = 0;
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
                sum += countAccumLoads(I->getOperand(i), Stmt, WriteMA);
            return sum;
        }
        return 0; // constants, args, etc.
    }

    // For the stmt's self-RAW map, find which domain dimensions carry the
    // dependence (i.e., the d-th delta is not pinned to zero). For matmul this
    // returns {k}; for a scalar reduction over a 2D loop, returns {i, j}; for
    // a prefix-sum scan over i, returns {i}.
    static std::vector<unsigned> findCarrierAxes(const isl::map &selfRAW)
    {
        std::vector<unsigned> axes;
        if (selfRAW.is_null()) return axes;

        isl::set deltas = selfRAW.deltas();
        unsigned n = isl_set_dim(deltas.get(), isl_dim_set);

        for (unsigned d = 0; d < n; ++d) {
            isl_set *proj = isl_set_copy(deltas.get());
            if (n > d + 1)
                proj = isl_set_project_out(proj, isl_dim_set, d + 1, n - d - 1);
            if (d > 0)
                proj = isl_set_project_out(proj, isl_dim_set, 0, d);

            // proj is now 1-dim. Check whether it contains only the value 0.
            isl_space *sp = isl_set_get_space(proj);
            isl_local_space *ls = isl_local_space_from_space(sp);
            isl_aff *a = isl_aff_var_on_domain(ls, isl_dim_set, 0);
            isl_constraint *c = isl_equality_from_aff(a);
            isl_basic_set *bs = isl_basic_set_universe(isl_set_get_space(proj));
            bs = isl_basic_set_add_constraint(bs, c);
            isl_set *zeroSet = isl_set_from_basic_set(bs);
            isl_bool isAllZero = isl_set_is_subset(proj, zeroSet);
            isl_set_free(zeroSet);
            isl_set_free(proj);

            if (isAllZero != isl_bool_true)
                axes.push_back(d);
        }
        return axes;
    }

    static ClassifyResult classifyStmt(polly::ScopStmt &Stmt,
                                       const polly::Dependences &Deps)
    {
        ClassifyResult R{spatial, {}};

        // Find the single array-kind write. Note Polly's own reduction hint
        // for cross-checking.
        polly::MemoryAccess *WriteMA = nullptr;
        int writeCount = 0;
        bool pollyReductionLike = false;
        for (polly::MemoryAccess *MA : Stmt) {
            if (!MA->isArrayKind()) continue;
            if (MA->isWrite()) { WriteMA = MA; writeCount++; }
            if (MA->isReductionLike()) pollyReductionLike = true;
        }
        const char *raw_name = isl_set_get_tuple_name(Stmt.getDomain().get());
        std::string name = raw_name ? raw_name : "";

        if (writeCount != 1) {
            errs() << "      [classifyStmt] " << name
                   << ": " << writeCount << " array writes -- classifying as spatial\n";
            return R;
        }

        // Property 1: self-{RAW, WAR, WAW}.
        isl::union_map raw = Deps.getDependences(polly::Dependences::TYPE_RAW);
        isl::union_map war = Deps.getDependences(polly::Dependences::TYPE_WAR);
        isl::union_map waw = Deps.getDependences(polly::Dependences::TYPE_WAW);
        isl::map selfRAW = extractSelfMap(raw, name);
        isl::map selfWAR = extractSelfMap(war, name);
        isl::map selfWAW = extractSelfMap(waw, name);
        bool prop1 = !selfRAW.is_null() && !selfWAR.is_null() && !selfWAW.is_null();

        // Sanity-check Polly's own detector. Disagreement is a warning, not an
        // error: Polly's isReductionLike is conservative and known to miss
        // matmul-style += patterns.
        if (pollyReductionLike != prop1) {
            errs() << "      [classifyStmt] " << name
                   << ": Polly reductionLike=" << pollyReductionLike
                   << " disagrees with prop1=" << prop1
                   << " (using our analysis)\n"
				   << "we can see (bool): \n"
				   << "RAW: " << !selfRAW.is_null() 
				   << ", WAR: " << !selfWAR.is_null() 
				   << ", WAW: " << !selfWAW.is_null();
        }

        if (!prop1) return R; // spatial

        // Property 3: exactly one read of the accumulator location in the RHS.
        auto *StoreI = llvm::dyn_cast<llvm::StoreInst>(WriteMA->getAccessInstruction());
        if (!StoreI) {
            errs() << "      [classifyStmt] " << name
                   << ": write is not a StoreInst -- classifying as spatial\n";
            return R;
        }
        int accumLoads = countAccumLoads(StoreI->getValueOperand(), &Stmt, WriteMA);
        if (accumLoads != 1) {
            throw std::runtime_error("LaTensor: Failed to process " + name +
                ": complicated reduction (accumulator read " +
                std::to_string(accumLoads) + " times in RHS, expected 1)");
        }

        // Carrying axes from the self-RAW deltas.
        R.red_axes = findCarrierAxes(selfRAW);

        // Property 2: outgoing RAW to another stmt => scan.
        if (hasOutgoingDepToOther(raw, name)) {
            if (R.red_axes.size() != 1) {
                errs() << "      [classifyStmt] " << name
                       << ": scan with " << R.red_axes.size()
                       << " carrying axes -- outside scalar-accumulator assumption\n";
            }
            R.stmt_type = scan;
        } else {
            R.stmt_type = reduction;
        }
        return R;
    }

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

                std::map<polly::ScopStmt *, ClassifyResult> stmt_classification{};

                for (polly::ScopStmt &Stmt: *S)
                {
                    errs() << "    Stmt: " << Stmt.getBaseName() << "\n";

                    isl::set Domain = Stmt.getDomain();
                    char *DomainStr = isl_set_to_str(Domain.get());
                    errs() << "      Loop Bounds (Domain): " << DomainStr << "\n";
                    free(DomainStr);

                    for (Instruction *I: Stmt.getInstructions())
                        errs() << "      Inst: " << *I << "\n";

                    for (polly::MemoryAccess *MA: Stmt) {
                        errs() << "      MemAccess: ";
                        MA->print(errs());
                        errs() << "\n";
                    }

                    try {
                        ClassifyResult cr = classifyStmt(Stmt, Deps);
                        stmt_classification[&Stmt] = cr;
                        const char *kind = (cr.stmt_type == scan) ? "scan"
                                         : (cr.stmt_type == reduction) ? "reduction"
                                         : "spatial";
                        errs() << "      => " << kind;
                        if (!cr.red_axes.empty()) {
                            errs() << ", carrying dims [";
                            for (size_t i = 0; i < cr.red_axes.size(); ++i)
                                errs() << (i ? ", " : "") << cr.red_axes[i];
                            errs() << "]";
                        }
                        errs() << "\n";
                    } catch (const std::runtime_error &e) {
                        errs() << "      ERROR: " << e.what() << "\n";
                    }
                }

                S->print(llvm::errs(), false);
            }

            // 4. Return the analyses preserved (all of them, since we only read)
            return PreservedAnalyses::all();
        }

        // The main recursive walker
        std::vector<std::shared_ptr<TreeNode>>
        walkAST(isl::ast_node Node, std::vector<std::shared_ptr<LoopInfo>> loop_backtrace)
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

                for (const std::shared_ptr<LoopInfo> &li: loop_backtrace)
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
