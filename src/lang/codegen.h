// Generación de bytecode a partir del AST anotado por el analizador.
#pragma once

#include "analyzer.h"
#include "vm.h"

namespace hz {

class CodeGen {
public:
    CodeGen(VM& vm, ModuleUnit& unit);
    ObjFunction* compileScript();

private:
    VM& vm;
    ModuleUnit& unit;
    DiagnosticBag& diags;

    struct LoopCtx {
        int start = 0;
        int breakDepth = 0;     // número de locales vivos al que se vuelve con break
        int continueDepth = 0;  // número de locales vivos al que se vuelve con continue
        std::vector<int> breaks, continues;
        int tryDepth = 0;
    };
    struct TryCtx { bool handlerActive; FuncDecl* finallyFn; };
    struct FnState {
        ObjFunction* fn = nullptr;
        FuncDecl* decl = nullptr;
        std::vector<bool> live;  // locales vivos: capturado?
        std::unordered_map<int, bool> capturedHint;
        std::vector<LoopCtx> loops;
        std::vector<TryCtx> tries;
        SrcLoc lastLoc;
    };
    FnState* fs = nullptr;
    std::vector<int>* chainJumps = nullptr;
    std::unordered_map<std::string, int> stringConsts;

    // Emisión
    void op(uint8_t o, SrcLoc loc = {});
    void u8(uint8_t v);
    void propOp(uint8_t o, int name, SrcLoc loc);
    void u16(int v);
    int emitJump(uint8_t o, SrcLoc loc = {});
    void patchJump(int at);
    void patchJumpTo(int at, int target);
    void emitLoop(int start, SrcLoc loc = {});
    int here() const { return (int)fs->fn->code.size(); }
    int addConst(Value v);
    int nameConst(const std::string& s);
    void setLoc(SrcLoc loc);

    // Locales
    void declareLocal(int expectedSlot, bool captured, SrcLoc loc);
    void popLocalsTo(int n);            // emite y actualiza
    void emitPopsTo(int n);             // emite sin actualizar (break/continue/return)
    void registerBlock(BlockStmt* b);
    void exitTries(int toDepth);
    void callFinally(FuncDecl* f, SrcLoc loc);

    // Funciones
    ObjFunction* compileFunction(FuncDecl* f, ClassStmt* owner, bool isFieldInit = false, ClassStmt* fieldsOf = nullptr);
    void emitClosure(FuncDecl* f, ClassStmt* owner, SrcLoc loc, bool isFieldInit = false, ClassStmt* fieldsOf = nullptr);

    // Sentencias y expresiones
    void stmt(Stmt* s);
    void block(BlockStmt* b);
    void classDecl(ClassStmt* c);
    void expr(Expr* e);
    void call(CallExpr* c);
    void load(const Resolution& r, const std::string& name, SrcLoc loc);
    void store(const Resolution& r, const std::string& name, SrcLoc loc); // valor en la cima (lo deja)
    void assign(AssignExpr* a, bool discard);
    void incDec(IncDecExpr* e, bool discard);
    void args(std::vector<Arg>& args, SrcLoc loc, std::function<void(int argc, int namesConst, bool spread)> finish);
    void matchExpr(MatchExpr* m);
    uint8_t binOp(Tok t);
};

} // namespace hz
