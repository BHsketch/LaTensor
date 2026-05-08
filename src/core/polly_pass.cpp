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
        std::vector<AxisMultiplier> linear_combination{};
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

    std::string buildMathematicalAccess(polly::MemoryAccess *MA)
    {
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
        isl_pw_multi_aff_foreach_piece(pma_c, [](isl_set *set, isl_multi_aff *m_aff, void *user) -> isl_stat
        {
            isl_multi_aff **ma_ptr = static_cast<isl_multi_aff **>(user);
            if (!*ma_ptr)
            {
                *ma_ptr = isl_multi_aff_copy(m_aff); // Copy the first math piece we find
            }
            isl_set_free(set);
            isl_multi_aff_free(m_aff);
            return isl_stat_ok;
        }, &ma);

        if (ma)
        {
            // We want the math for the 1st dimension of the array access (Index 0)
            // E.g., for MemRef1[128 * i0 + i1], this gets the (128 * i0 + i1) expression
            isl_aff *aff = isl_multi_aff_get_aff(ma, 0);

            // How many loop iterators (i0, i1, i2) does this math use?
            int num_in_dims = isl_aff_dim(aff, isl_dim_in);

            bool is_first = true;

            // 3. EXTRACT THE COEFFICIENTS (e.g., the '128' in 128*vi0)
            for (int i = 0; i < num_in_dims; ++i)
            {
                // Get the integer multiplier for dimension 'i'
                isl_val *coef_val = isl_aff_get_coefficient_val(aff, isl_dim_in, i);
                long coef = isl_val_get_num_si(coef_val);
                isl_val_free(coef_val);

                if (coef != 0)
                {
                    if (!is_first) index_expr += " + ";

                    if (coef == 1)
                    {
                        index_expr += "vi" + std::to_string(i);
                    }
                    else
                    {
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

            if (const_term != 0)
            {
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

    struct ClassifyResult
    {
        polly::MemoryAccess *write;     // which write this entry describes
        IterVarType stmt_type;          // spatial | reduction | scan
        std::vector<unsigned> red_axes; // domain dim indices carrying the reduction/scan dependence
    };

    // Filter a stmt-level union_map down to the union of all maps whose source
    // AND sink tuples equal `name`. Returns null isl::map if no such map exists.
    static isl::map extractSelfMap(const isl::union_map &UM, const std::string &name)
    {
        struct Ctx
        {
            std::string name;
            isl_map *result;
        };
        Ctx ctx{name, nullptr};
        isl_union_map_foreach_map(UM.get(),
                                  [](isl_map *m, void *user) -> isl_stat
                                  {
                                      auto *c = static_cast<Ctx *>(user);
                                      const char *src = isl_map_get_tuple_name(m, isl_dim_in);
                                      const char *snk = isl_map_get_tuple_name(m, isl_dim_out);
                                      if (src && snk && c->name == src && c->name == snk)
                                      {
                                          c->result = c->result ? isl_map_union(c->result, m) : m;
                                      }
                                      else
                                      {
                                          isl_map_free(m);
                                      }
                                      return isl_stat_ok;
                                  }, &ctx);
        return ctx.result ? isl::manage(ctx.result) : isl::map();
    }

    // Get the access-tag name used for `MA` in AL_Access dep maps.
    static std::string getAccName(polly::MemoryAccess *MA)
    {
        isl::id id = MA->getId();
        if (id.is_null()) return "";
        const char *n = isl_id_get_name(id.get());
        return n ? std::string(n) : "";
    }

    // Filter a union_map of AL_Access deps. Polly mixes two map shapes within
    // the same union_map:
    //   - Wrapped (per-access):  [Stmt[i] -> Acc[]] -> [Stmt[j] -> Acc[]]
    //   - Plain   (per-stmt):    Stmt[i] -> Stmt[j]
    // Wrapped maps appear for true array accesses; plain maps appear for
    // scalar/synthetic accesses or for scalar-promoted reductions Polly
    // recognized internally (e.g., reduction_2d's S[0] reduction lives only
    // in plain TYPE_RED).
    //
    // Wrapped maps are filtered by access tag and then projected to plain
    // Stmt[i] -> Stmt[j] form. Plain maps have no access tags; if `acceptPlain`
    // is true they are kept based only on stmt-name constraints (and only when
    // no sinkAccNot constraint is set, since we can't verify it on a plain map).
    //
    // Empty `srcAcc` / `sinkAcc` / `sinkAccNot` => no constraint on that field.
    // `sinkStmtMustBeSame` controls whether sink stmt must equal `stmtName`
    // (true; intra-stmt deps) or merely exist (false; any stmt -- outgoing
    // queries).
    static isl::union_map filterAccessDeps(const isl::union_map &UM,
                                           const std::string &stmtName,
                                           const std::string &srcAcc,
                                           const std::string &sinkAcc,
                                           const std::string &sinkAccNot,
                                           bool sinkStmtMustBeSame,
                                           bool acceptPlain)
    {
        struct Ctx
        {
            const std::string *stmtName;
            const std::string *srcAcc;
            const std::string *sinkAcc;
            const std::string *sinkAccNot;
            bool sinkStmtMustBeSame;
            bool acceptPlain;
            isl_union_map *out;
        };
        isl_ctx *ictx = isl_union_map_get_ctx(UM.get());
        Ctx ctx{&stmtName, &srcAcc, &sinkAcc, &sinkAccNot, sinkStmtMustBeSame,
                acceptPlain,
                isl_union_map_empty(isl_space_params_alloc(ictx, 0))};

        isl_union_map_foreach_map(UM.get(),
                                  [](isl_map *m, void *user) -> isl_stat
                                  {
                                      auto *c = static_cast<Ctx *>(user);

                                      isl_space *sp = isl_map_get_space(m);
                                      isl_space *dom_sp = isl_space_domain(isl_space_copy(sp));
                                      isl_space *rng_sp = isl_space_range(sp);

                                      bool wrapped = isl_space_is_wrapping(dom_sp) && isl_space_is_wrapping(rng_sp);

                                      if (!wrapped)
                                      {
                                          isl_space_free(dom_sp);
                                          isl_space_free(rng_sp);
                                          if (!c->acceptPlain)
                                          {
                                              isl_map_free(m);
                                              return isl_stat_ok;
                                          }
                                          // Plain map (no access tags). We can't verify access-tag
                                          // constraints, so we keep the dep based on stmt names alone
                                          // and rely on prop3 / IR-level checks downstream to filter
                                          // out wrong attributions to non-accumulator writes.
                                          const char *src = isl_map_get_tuple_name(m, isl_dim_in);
                                          const char *snk = isl_map_get_tuple_name(m, isl_dim_out);
                                          bool keep = src && *c->stmtName == src;
                                          if (keep)
                                          {
                                              bool stmt_eq = snk && *c->stmtName == snk;
                                              keep = c->sinkStmtMustBeSame ? stmt_eq : (bool) snk;
                                          }
                                          if (keep) c->out = isl_union_map_add_map(c->out, m);
                                          else isl_map_free(m);
                                          return isl_stat_ok;
                                      }

                                      isl_space *dom_uw = isl_space_unwrap(dom_sp);
                                      isl_space *rng_uw = isl_space_unwrap(rng_sp);

                                      const char *src_stmt = isl_space_get_tuple_name(dom_uw, isl_dim_in);
                                      const char *src_acc = isl_space_get_tuple_name(dom_uw, isl_dim_out);
                                      const char *snk_stmt = isl_space_get_tuple_name(rng_uw, isl_dim_in);
                                      const char *snk_acc = isl_space_get_tuple_name(rng_uw, isl_dim_out);

                                      bool keep = src_stmt && *c->stmtName == src_stmt;
                                      if (keep)
                                      {
                                          bool stmt_eq = snk_stmt && *c->stmtName == snk_stmt;
                                          keep = c->sinkStmtMustBeSame ? stmt_eq : (bool) snk_stmt;
                                      }
                                      if (keep && !c->srcAcc->empty()) keep = src_acc && *c->srcAcc == src_acc;
                                      if (keep && !c->sinkAcc->empty()) keep = snk_acc && *c->sinkAcc == snk_acc;
                                      if (keep && !c->sinkAccNot->empty()) keep = snk_acc && *c->sinkAccNot != snk_acc;

                                      isl_space_free(dom_uw);
                                      isl_space_free(rng_uw);

                                      if (keep)
                                      {
                                          // [Stmt[i] -> Acc[]] -> [Stmt[j] -> Acc[]]
                                          //   --domain_factor_domain--> Stmt[i] -> [Stmt[j] -> Acc[]]
                                          //   --range_factor_domain---> Stmt[i] -> Stmt[j]
                                          isl_map *step1 = isl_map_domain_factor_domain(isl_map_copy(m));
                                          isl_map *step2 = isl_map_range_factor_domain(step1);
                                          c->out = isl_union_map_add_map(c->out, step2);
                                      }
                                      isl_map_free(m);
                                      return isl_stat_ok;
                                  }, &ctx);

        return isl::manage(ctx.out);
    }

    // Walk Val's expression tree; count LoadInsts (or phi nodes for scalar
    // accumulators) whose Polly MemoryAccess has an access relation equal to
    // WriteMA's -- i.e., reads the accumulator cell. PHI nodes are treated as
    // LEAVES (never recursed into) because (a) for scalar reductions the phi
    // itself is the accumulator value and (b) recursing into a phi's operands
    // loops via the back-edge.
    static int countAccumLoads(llvm::Value *Val,
                               polly::ScopStmt *Stmt,
                               polly::MemoryAccess *WriteMA)
    {
        if (!Val) return 0;
        if (auto *Load = llvm::dyn_cast<llvm::LoadInst>(Val))
        {
            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->getAccessInstruction() == Load)
                {
                    return MA->getAccessRelation().is_equal(WriteMA->getAccessRelation()) ? 1 : 0;
                }
            }
            return 0;
        }
        if (auto *Phi = llvm::dyn_cast<llvm::PHINode>(Val))
        {
            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->getAccessInstruction() == Phi)
                {
                    return MA->getAccessRelation().is_equal(WriteMA->getAccessRelation()) ? 1 : 0;
                }
            }
            return 0;
        }
        if (auto *I = llvm::dyn_cast<llvm::Instruction>(Val))
        {
            int sum = 0;
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
                sum += countAccumLoads(I->getOperand(i), Stmt, WriteMA);
            return sum;
        }
        return 0; // constants, args, etc.
    }

    // Walk Val's expression tree; return the first Instruction (LoadInst or
    // PHINode) whose Polly MemoryAccess has an access relation equal to
    // WriteMA's. PHI nodes are treated as leaves (see countAccumLoads).
    static llvm::Instruction *findAccumLoadInTree(llvm::Value *Val,
                                                  polly::ScopStmt *Stmt,
                                                  polly::MemoryAccess *WriteMA)
    {
        if (!Val) return nullptr;
        if (auto *Load = llvm::dyn_cast<llvm::LoadInst>(Val))
        {
            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->getAccessInstruction() == Load)
                {
                    return MA->getAccessRelation().is_equal(WriteMA->getAccessRelation())
                           ? (llvm::Instruction *) Load : nullptr;
                }
            }
            return nullptr;
        }
        if (auto *Phi = llvm::dyn_cast<llvm::PHINode>(Val))
        {
            for (polly::MemoryAccess *MA: *Stmt)
            {
                if (MA->getAccessInstruction() == Phi)
                {
                    return MA->getAccessRelation().is_equal(WriteMA->getAccessRelation())
                           ? (llvm::Instruction *) Phi : nullptr;
                }
            }
            return nullptr;
        }
        if (auto *I = llvm::dyn_cast<llvm::Instruction>(Val))
        {
            for (unsigned i = 0; i < I->getNumOperands(); ++i)
                if (auto *got = findAccumLoadInTree(I->getOperand(i), Stmt, WriteMA))
                    return got;
        }
        return nullptr;
    }

    // Look up the MemoryAccess associated with a given access-instruction
    // (LoadInst for arrays, PHINode for scalar accumulators) within Stmt.
    static polly::MemoryAccess *getMAForAccessInst(polly::ScopStmt *Stmt, llvm::Instruction *I)
    {
        if (!I) return nullptr;
        for (polly::MemoryAccess *MA: *Stmt)
            if (MA->getAccessInstruction() == I) return MA;
        return nullptr;
    }

    // For a (possibly per-write-restricted) self-RAW map, find which domain
    // dimensions carry the dependence (i.e., the d-th delta is not pinned to
    // zero). For matmul this returns {k}; for a scalar reduction over a 2D loop,
    // returns {i, j}; for a prefix-sum scan over i, returns {i}.
    static std::vector<unsigned> findCarrierAxes(const isl::map &selfRAW)
    {
        std::vector<unsigned> axes;
        if (selfRAW.is_null() || selfRAW.is_empty()) return axes;

        isl::set deltas = selfRAW.deltas();
        unsigned n = isl_set_dim(deltas.get(), isl_dim_set);

        for (unsigned d = 0; d < n; ++d)
        {
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

    // Decide whether a write should be considered for reduction/scan classification.
    // Skip read-only accesses and ExitPHI (loop-exit live-outs, not in-loop accums).
    static bool isCandidateWrite(polly::MemoryAccess *MA)
    {
        if (!MA->isWrite()) return false;
        if (MA->getOriginalKind() == polly::MemoryKind::ExitPHI) return false;
        return true;
    }

    static std::vector<ClassifyResult>
    classifyStmt(polly::ScopStmt &Stmt, const polly::Dependences &Deps)
    {
        std::vector<ClassifyResult> results;

        const char *raw_name = isl_set_get_tuple_name(Stmt.getDomain().get());
        std::string name = raw_name ? raw_name : "";

        // Collect every write we'll consider (arrays + scalar accumulators).
        // if we have a computation such as A[i] = A[i] + B[j], then
        // a "MemoryAccess" is basically one single "A[i]" or "B[j]" from this
        // computation
        std::vector<polly::MemoryAccess *> writes;
        bool pollyReductionLike = false;
        for (polly::MemoryAccess *MA: Stmt)
        {
            if (isCandidateWrite(MA)) writes.push_back(MA);
            if (MA->isArrayKind() && MA->isReductionLike()) pollyReductionLike = true;
        }

        if (writes.empty())
        {
            errs() << "      [classifyStmt] " << name << ": no candidate writes\n";
            return results;
        }

        // Pre-fetch the four dep maps once. These come at AL_Access granularity:
        // each map has wrapped tuples [Stmt[i] -> Acc[]] -> [Stmt[j] -> Acc[]].
        // TYPE_RED holds reduction-pattern deps that Polly stripped from the
        // standard RAW/WAR/WAW buckets when it recognized a clean reduction.
        isl::union_map raw = Deps.getDependences(polly::Dependences::TYPE_RAW);
        isl::union_map war = Deps.getDependences(polly::Dependences::TYPE_WAR);
        isl::union_map waw = Deps.getDependences(polly::Dependences::TYPE_WAW);
        isl::union_map red = Deps.getDependences(polly::Dependences::TYPE_RED);

        for (polly::MemoryAccess *WriteMA: writes)
        {
            ClassifyResult R{WriteMA, spatial, {}};
            std::string accW = getAccName(WriteMA);

            // --- Property 1: self-{RAW,WAR,WAW} on this specific write,
            // OR any reduction-bucket dep involving this write. ---
            // At AL_Access we filter by access tag rather than by access-
            // relation domain restriction, which can't disambiguate two
            // writes that share the same iteration space. Plain (un-tagged)
            // deps are also accepted -- Polly emits these for scalar /
            // scalar-promoted accesses, e.g., reduction_2d's S[0] reduction
            // lives only in plain TYPE_RED. Misattribution to the wrong
            // write is filtered out by prop3.
            isl::union_map rawW = filterAccessDeps(raw, name, accW, "", "", true,  /*acceptPlain=*/true);
            isl::union_map wawW = filterAccessDeps(waw, name, accW, "", "", true,  /*acceptPlain=*/true);
            isl::union_map warW = filterAccessDeps(war, name, "", accW, "", true, /*acceptPlain=*/true);
            isl::union_map redSrcW = filterAccessDeps(red, name, accW, "", "", true, /*acceptPlain=*/true);
            isl::union_map redSinkW = filterAccessDeps(red, name, "", accW, "", true, /*acceptPlain=*/true);

            isl::map selfRAW = extractSelfMap(rawW, name);
            isl::map selfWAR = extractSelfMap(warW, name);
            isl::map selfWAW = extractSelfMap(wawW, name);
            isl::map selfREDsrc = extractSelfMap(redSrcW, name);
            isl::map selfREDsnk = extractSelfMap(redSinkW, name);

            bool hasRAW = !selfRAW.is_null() && !selfRAW.is_empty();
            bool hasWAR = !selfWAR.is_null() && !selfWAR.is_empty();
            bool hasWAW = !selfWAW.is_null() && !selfWAW.is_empty();
            bool hasRED = (!selfREDsrc.is_null() && !selfREDsrc.is_empty())
                          || (!selfREDsnk.is_null() && !selfREDsnk.is_empty());

            // Either Polly recognized this write as part of a reduction
            // (deps live in TYPE_RED) OR all three classical AK self-deps
            // are present (handles complicated/non-Polly-recognized cases).
            bool prop1 = hasRED || (hasRAW && hasWAR && hasWAW);

            // Cross-check Polly only for array-kind writes; scalar MKs make this noisy.
            if (WriteMA->isArrayKind() && pollyReductionLike != prop1)
            {
                errs() << "      [classifyStmt] " << name
                       << " (write @ " << WriteMA << "): Polly reductionLike="
                       << pollyReductionLike << " disagrees with prop1=" << prop1
                       << " (using our analysis)\n"
                       << "        RAW=" << hasRAW << " WAR=" << hasWAR
                       << " WAW=" << hasWAW << " RED=" << hasRED << "\n";
            }

            if (!prop1)
            {
                results.push_back(R); // spatial
                continue;
            }

            // --- Property 3: exactly one read of THIS accumulator location
            // in the RHS. Also remember the accumulator-read MA so prop2
            // can exclude it. ---
            polly::MemoryAccess *accumReadMA = nullptr;
            auto *StoreI = llvm::dyn_cast<llvm::StoreInst>(WriteMA->getAccessInstruction());
            if (!StoreI)
            {
                // Scalar/synthetic write (MK_Value/MK_PHI). Walk every
                // instruction in the stmt for accumulator loads.
                int accumLoads = 0;
                for (llvm::Instruction *I: Stmt.getInstructions())
                    accumLoads += countAccumLoads(I, &Stmt, WriteMA);
                if (accumLoads != 1)
                {
                    errs() << "      [classifyStmt] " << name
                           << " (write @ " << WriteMA << "): scalar/synthetic write with "
                           << accumLoads << " accumulator reads -- skipping (spatial)\n";
                    results.push_back(R);
                    continue;
                }
                for (llvm::Instruction *I: Stmt.getInstructions())
                    if (auto *L = findAccumLoadInTree(I, &Stmt, WriteMA))
                    {
                        accumReadMA = getMAForAccessInst(&Stmt, L);
                        break;
                    }
            }
            else
            {
                llvm::Value *RHS = StoreI->getValueOperand();
                int accumLoads = countAccumLoads(RHS, &Stmt, WriteMA);
                if (accumLoads != 1)
                {
                    errs() << "      [classifyStmt] " << name
                           << " (write @ " << WriteMA << "): complicated reduction "
                           << "(accumulator read " << accumLoads
                           << " times in RHS, expected 1) -- skipping (spatial)\n";
                    results.push_back(R);
                    continue;
                }
                accumReadMA = getMAForAccessInst(&Stmt, findAccumLoadInTree(RHS, &Stmt, WriteMA));
            }

            // Carrier axes: prefer selfRED (when Polly recognized and
            // stripped the deps), fall back to selfRAW.
            const isl::map &carrierMap =
                    (!selfREDsrc.is_null() && !selfREDsrc.is_empty()) ? selfREDsrc : selfRAW;
            R.red_axes = findCarrierAxes(carrierMap);

            // --- Property 2: scan iff there's any consumer of the partial
            // result OTHER than the accumulator's own re-read.
            //
            // Two complementary checks:
            //   (a) Dep-map: a wrapped RAW from this write to a non-
            //       accumulator sink access, in any stmt. Catches
            //       inter-stmt scans where Polly kept producer/consumer
            //       separate.
            //   (b) IR-level: the accumulator-read load has more uses than
            //       the one feeding its own store. Catches intra-stmt
            //       scans where Polly merged producer & consumer into one
            //       stmt (and early-cse coalesced both reads into one
            //       LoadInst, so Polly's flow analysis sees only the
            //       accumulator-pattern dep).
            bool prop2_scan = false;

            std::string accumReadName = accumReadMA ? getAccName(accumReadMA) : "";
            isl::union_map outRAW = filterAccessDeps(raw, name, accW, "", accumReadName,
                    /*sinkStmtMustBeSame=*/false, /*acceptPlain=*/false);
            if (!outRAW.is_empty()) prop2_scan = true;

            // IR-level check (a): the stored value (combiner output) has
            // users besides this store. After early-cse, a post-store
            // re-read of the accumulator location is replaced by the
            // stored value -- so an external consumer of "the new partial
            // result" shows up as an extra user of StoreI's value operand.
            if (!prop2_scan && StoreI)
            {
                if (auto *NewVal = llvm::dyn_cast<llvm::Instruction>(
                        StoreI->getValueOperand()))
                {
                    if (std::distance(NewVal->user_begin(),
                                      NewVal->user_end()) != 1)
                    {
                        prop2_scan = true;
                    }
                }
            }

            // IR-level check (b): the accumulator-read load has users
            // besides feeding the combiner. Catches the producer-after-
            // consumer ordering where the consumer reads the *pre-write*
            // accumulator value.
            if (!prop2_scan && StoreI && accumReadMA)
            {
                if (auto *AccumLoad = llvm::dyn_cast<llvm::LoadInst>(
                        accumReadMA->getAccessInstruction()))
                {
                    if (std::distance(AccumLoad->user_begin(),
                                      AccumLoad->user_end()) != 1)
                    {
                        prop2_scan = true;
                    }
                }
            }

            if (prop2_scan)
            {
                if (R.red_axes.size() != 1)
                {
                    errs() << "      [classifyStmt] " << name
                           << " (write @ " << WriteMA << "): scan with "
                           << R.red_axes.size()
                           << " carrying axes -- outside scalar-accumulator assumption\n";
                }
                R.stmt_type = scan;
            }
            else
            {
                R.stmt_type = reduction;
            }

            results.push_back(R);
        }

        return results;
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
                errs() << "TVM CODE IS:\n" << LoopTree[0]->to_string() + "";

                // Now LoopTree contains your fully structured, nested hierarchy.

                // 1. Let auto resolve the exact wrapper type returned by the Pass Manager
                auto &DI = SAM.getResult<polly::DependenceAnalysis>(*S, AR);

                // 2. Get the Dependences analysis object at the per-access level.
                // AL_Access is required so we can disambiguate which write within
                // a multi-write ScopStmt produced each dependence -- crucial for
                // detecting scans where Polly merged producer and consumer into
                // a single ScopStmt.
                const polly::Dependences &Deps = DI.getDependences(polly::Dependences::AL_Access);

                // 3. Extract the actual ISL union_map for the True (RAW) dependencies
                isl::union_map TrueDeps = Deps.getDependences(polly::Dependences::TYPE_RAW);

                std::map<polly::ScopStmt *, std::vector<ClassifyResult>> stmt_classification{};

                for (polly::ScopStmt &Stmt: *S)
                {
                    errs() << "    Stmt: " << Stmt.getBaseName() << "\n";

                    isl::set Domain = Stmt.getDomain();
                    char *DomainStr = isl_set_to_str(Domain.get());
                    errs() << "      Loop Bounds (Domain): " << DomainStr << "\n";
                    free(DomainStr);

                    for (Instruction *I: Stmt.getInstructions())
                        errs() << "      Inst: " << *I << "\n";

                    for (polly::MemoryAccess *MA: Stmt)
                    {
                        errs() << "      MemAccess: ";
                        MA->print(errs());
                        errs() << "\n";
                    }

                    try
                    {
                        std::vector<ClassifyResult> cr = classifyStmt(Stmt, Deps);
                        for (const auto &r: cr)
                        {
                            const char *kind = (r.stmt_type == scan) ? "scan"
                                                                     : (r.stmt_type == reduction) ? "reduction"
                                                                                                  : "spatial";
                            errs() << "      => write @ " << r.write << ": " << kind;
                            if (!r.red_axes.empty())
                            {
                                errs() << ", carrying dims [";
                                for (size_t i = 0; i < r.red_axes.size(); ++i)
                                    errs() << (i ? ", " : "") << r.red_axes[i];
                                errs() << "]";
                            }
                            errs() << "\n";
                        }
                        if (!cr.empty())
                            stmt_classification[&Stmt] = std::move(cr);
                    }
                    catch (const std::runtime_error &e)
                    {
                        errs() << "      ERROR: " << e.what() << "\n";
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
