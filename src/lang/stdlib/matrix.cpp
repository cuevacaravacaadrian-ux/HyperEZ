// Matrix: álgebra lineal en C++ (rápida) para inteligencia artificial, ciencia de datos y gráficos.
// Admite +, -, * y / (elemento a elemento o con un número), producto de matrices (dot),
// transpuesta, funciones de activación y reducciones. Las filas o columnas de 1 se expanden
// automáticamente ("broadcasting"): una matriz n×m más una fila 1×m suma la fila a cada fila.
#include "lang/stdlib/native.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace hz {

struct Mat {
    int rows = 0, cols = 0;
    std::vector<double> d;
    Mat() = default;
    Mat(int r, int c, double v = 0) : rows(r), cols(c), d((size_t)r * c, v) {}
    double& at(int i, int j) { return d[(size_t)i * cols + j]; }
    double at(int i, int j) const { return d[(size_t)i * cols + j]; }
};

static void freeMat(void* p) { delete static_cast<Mat*>(p); }
static ObjClass* matrixClass(VM& vm) { return static_cast<ObjClass*>(vm.builtins[vm.builtinSlot("Matrix")].o); }
static bool isMatrix(VM& vm, const Value& v) { return isObjType(v, OT::Foreign) && static_cast<ObjForeign*>(v.o)->cls == matrixClass(vm); }
static Mat& M(Value* args) { return *static_cast<Mat*>(static_cast<ObjForeign*>(args[-1].o)->ptr); }
static Mat& asMat(VM& vm, const Value& v, const char* fname) {
    if (!isMatrix(vm, v)) vm.raise(std::string(fname) + ": se esperaba una Matrix, no " + vm.typeName(v));
    return *static_cast<Mat*>(static_cast<ObjForeign*>(v.o)->ptr);
}
static Value wrap(VM& vm, Mat m) { return Value::object(vm.newForeign(matrixClass(vm), new Mat(std::move(m)), freeMat)); }
static int dim(VM& vm, Value* args, int argc, int i, const char* fname, double def = NAN) {
    double d = argNum(vm, args, argc, i, fname, def);
    if (d < 0 || d > 1e8 || std::floor(d) != d) vm.raise(std::string(fname) + ": " + formatNumber(d) + " no es un tamaño válido");
    return (int)d;
}
static std::string shape(const Mat& m) { return std::to_string(m.rows) + "×" + std::to_string(m.cols); }

static Mat fromList(VM& vm, const Value& v, const char* fname) {
    if (!isObjType(v, OT::List)) vm.raise(std::string(fname) + ": se esperaba una lista de filas [[1, 2], [3, 4]]");
    auto& rows = static_cast<ObjList*>(v.o)->items;
    if (rows.empty()) return Mat(0, 0);
    // Una lista de números es una fila
    if (rows[0].isNum()) {
        Mat m(1, (int)rows.size());
        for (size_t j = 0; j < rows.size(); j++) {
            if (!rows[j].isNum()) vm.raise(std::string(fname) + ": todos los valores deben ser números");
            m.d[j] = rows[j].n;
        }
        return m;
    }
    int cols = -1;
    Mat m;
    for (size_t i = 0; i < rows.size(); i++) {
        if (!isObjType(rows[i], OT::List)) vm.raise(std::string(fname) + ": cada fila debe ser una lista de números");
        auto& r = static_cast<ObjList*>(rows[i].o)->items;
        if (cols < 0) { cols = (int)r.size(); m = Mat((int)rows.size(), cols); }
        if ((int)r.size() != cols) vm.raise(std::string(fname) + ": todas las filas deben tener " + std::to_string(cols) + " valores");
        for (int j = 0; j < cols; j++) {
            if (!r[j].isNum()) vm.raise(std::string(fname) + ": todos los valores deben ser números");
            m.at((int)i, j) = r[j].n;
        }
    }
    return m;
}

// Operación elemento a elemento con expansión de filas/columnas de tamaño 1
template <class F> static Mat broadcast(VM& vm, const Mat& a, const Mat& b, F f, const char* what) {
    int r = std::max(a.rows, b.rows), c = std::max(a.cols, b.cols);
    auto fits = [&](const Mat& m) { return (m.rows == r || m.rows == 1) && (m.cols == c || m.cols == 1); };
    if (!fits(a) || !fits(b)) vm.raise(std::string("Matrix: no se puede ") + what + " una matriz " + shape(a) + " y otra " + shape(b));
    Mat out(r, c);
    for (int i = 0; i < r; i++)
        for (int j = 0; j < c; j++)
            out.at(i, j) = f(a.at(a.rows == 1 ? 0 : i, a.cols == 1 ? 0 : j), b.at(b.rows == 1 ? 0 : i, b.cols == 1 ? 0 : j));
    return out;
}
template <class F> static Value binop(VM& vm, Value* args, int argc, F f, const char* what, const char* fname) {
    Mat& a = M(args);
    Value o = optArg(args, argc, 0);
    if (o.isNum()) {
        Mat out = a;
        for (auto& x : out.d) x = f(x, o.n);
        return wrap(vm, std::move(out));
    }
    return wrap(vm, broadcast(vm, a, asMat(vm, o, fname), f, what));
}
template <class F> static Value mapNative(VM& vm, Value* args, F f) {
    Mat out = M(args);
    for (auto& x : out.d) x = f(x);
    return wrap(vm, std::move(out));
}

NATIVE(c_matrix) {
    Value a = optArg(args, argc, 0);
    Mat m = isObjType(a, OT::List) ? fromList(vm, a, "Matrix")
                                   : Mat(dim(vm, args, argc, 0, "Matrix"), dim(vm, args, argc, 1, "Matrix", 1), argNum(vm, args, argc, 2, "Matrix", 0));
    return Value::object(vm.newForeign(static_cast<ObjClass*>(args[-1].o), new Mat(std::move(m)), freeMat));
}
NATIVE(m_rows) { UNUSED; return Value(M(args).rows); }
NATIVE(m_cols) { UNUSED; return Value(M(args).cols); }
NATIVE(m_size) { UNUSED; return Value((double)M(args).d.size()); }
static void checkIJ(VM& vm, const Mat& m, int i, int j, const char* fname) {
    if (i < 0 || j < 0 || i >= m.rows || j >= m.cols)
        vm.raise(std::string(fname) + ": la posición (" + std::to_string(i) + ", " + std::to_string(j) + ") está fuera de la matriz " + shape(m));
}
NATIVE(m_get) {
    Mat& m = M(args);
    int i = (int)argNum(vm, args, argc, 0, "Matrix.get"), j = (int)argNum(vm, args, argc, 1, "Matrix.get", 0);
    checkIJ(vm, m, i, j, "Matrix.get");
    return Value(m.at(i, j));
}
NATIVE(m_set) {
    Mat& m = M(args);
    int i = (int)argNum(vm, args, argc, 0, "Matrix.set"), j = (int)argNum(vm, args, argc, 1, "Matrix.set");
    checkIJ(vm, m, i, j, "Matrix.set");
    m.at(i, j) = argNum(vm, args, argc, 2, "Matrix.set");
    return Value();
}
NATIVE(m_row) {
    Mat& m = M(args);
    int i = (int)argNum(vm, args, argc, 0, "Matrix.row");
    checkIJ(vm, m, i, 0, "Matrix.row");
    ObjList* l = vm.newList();
    for (int j = 0; j < m.cols; j++) l->items.push_back(Value(m.at(i, j)));
    return Value::object(l);
}
NATIVE(m_col) {
    Mat& m = M(args);
    int j = (int)argNum(vm, args, argc, 0, "Matrix.col");
    checkIJ(vm, m, 0, j, "Matrix.col");
    ObjList* l = vm.newList();
    for (int i = 0; i < m.rows; i++) l->items.push_back(Value(m.at(i, j)));
    return Value::object(l);
}
NATIVE(m_toList) {
    UNUSED;
    Mat& m = M(args);
    ObjList* out = vm.newList();
    TempRoot keep(vm, out);
    for (int i = 0; i < m.rows; i++) {
        ObjList* r = vm.newList();
        out->items.push_back(Value::object(r));
        for (int j = 0; j < m.cols; j++) r->items.push_back(Value(m.at(i, j)));
    }
    return Value::object(out);
}
NATIVE(m_flatten) {
    UNUSED;
    ObjList* l = vm.newList();
    for (double x : M(args).d) l->items.push_back(Value(x));
    return Value::object(l);
}
NATIVE(m_copy) { UNUSED; return wrap(vm, M(args)); }
NATIVE(m_reshape) {
    Mat m = M(args);
    int r = dim(vm, args, argc, 0, "Matrix.reshape"), c = dim(vm, args, argc, 1, "Matrix.reshape");
    if ((size_t)r * c != m.d.size()) vm.raise("Matrix.reshape: " + shape(m) + " no cabe en " + std::to_string(r) + "×" + std::to_string(c));
    m.rows = r;
    m.cols = c;
    return wrap(vm, std::move(m));
}
NATIVE(m_add) { return binop(vm, args, argc, [](double a, double b) { return a + b; }, "sumar", "Matrix.add"); }
NATIVE(m_sub) { return binop(vm, args, argc, [](double a, double b) { return a - b; }, "restar", "Matrix.sub"); }
NATIVE(m_mul) { return binop(vm, args, argc, [](double a, double b) { return a * b; }, "multiplicar", "Matrix.mul"); }
NATIVE(m_div) { return binop(vm, args, argc, [](double a, double b) { return b == 0 ? 0.0 : a / b; }, "dividir", "Matrix.div"); }
NATIVE(m_pow) {
    double p = argNum(vm, args, argc, 0, "Matrix.pow");
    return mapNative(vm, args, [p](double x) { return std::pow(x, p); });
}
NATIVE(m_dot) {
    Mat& a = M(args);
    Mat& b = asMat(vm, optArg(args, argc, 0), "Matrix.dot");
    if (a.cols != b.rows) vm.raise("Matrix.dot: para multiplicar " + shape(a) + " por " + shape(b) + " las columnas de la primera deben ser las filas de la segunda");
    Mat out(a.rows, b.cols);
    // orden i-k-j: recorre la memoria en orden (mucho más rápido que i-j-k)
    for (int i = 0; i < a.rows; i++) {
        double* o = &out.d[(size_t)i * out.cols];
        for (int k = 0; k < a.cols; k++) {
            double x = a.at(i, k);
            if (x == 0) continue;
            const double* row = &b.d[(size_t)k * b.cols];
            for (int j = 0; j < b.cols; j++) o[j] += x * row[j];
        }
    }
    return wrap(vm, std::move(out));
}
NATIVE(m_transpose) {
    UNUSED;
    Mat& a = M(args);
    Mat out(a.cols, a.rows);
    for (int i = 0; i < a.rows; i++)
        for (int j = 0; j < a.cols; j++) out.at(j, i) = a.at(i, j);
    return wrap(vm, std::move(out));
}
NATIVE(m_map) {
    if (argc < 1) vm.raise("Matrix.map: falta la función");
    Mat out = M(args);
    for (auto& x : out.d) {
        Value v(x);
        Value r = vm.call(args[0], 1, &v);
        if (!r.isNum()) vm.raise("Matrix.map: la función debe devolver un número, no " + vm.typeName(r));
        x = r.n;
    }
    return wrap(vm, std::move(out));
}
NATIVE(m_sigmoid) { UNUSED; return mapNative(vm, args, [](double x) { return 1.0 / (1.0 + std::exp(-x)); }); }
NATIVE(m_relu) { UNUSED; return mapNative(vm, args, [](double x) { return x > 0 ? x : 0.0; }); }
NATIVE(m_reluDeriv) { UNUSED; return mapNative(vm, args, [](double x) { return x > 0 ? 1.0 : 0.0; }); }
NATIVE(m_tanh) { UNUSED; return mapNative(vm, args, [](double x) { return std::tanh(x); }); }
NATIVE(m_exp) { UNUSED; return mapNative(vm, args, [](double x) { return std::exp(x); }); }
NATIVE(m_log) { UNUSED; return mapNative(vm, args, [](double x) { return std::log(std::max(x, 1e-300)); }); }
NATIVE(m_abs) { UNUSED; return mapNative(vm, args, [](double x) { return std::fabs(x); }); }
NATIVE(m_sqrt) { UNUSED; return mapNative(vm, args, [](double x) { return std::sqrt(x); }); }
NATIVE(m_clip) {
    double lo = argNum(vm, args, argc, 0, "Matrix.clip"), hi = argNum(vm, args, argc, 1, "Matrix.clip");
    return mapNative(vm, args, [lo, hi](double x) { return std::clamp(x, lo, hi); });
}
NATIVE(m_softmax) {
    UNUSED;
    Mat out = M(args);
    for (int i = 0; i < out.rows; i++) {
        double mx = -INFINITY, sum = 0;
        for (int j = 0; j < out.cols; j++) mx = std::max(mx, out.at(i, j));
        for (int j = 0; j < out.cols; j++) sum += (out.at(i, j) = std::exp(out.at(i, j) - mx));
        for (int j = 0; j < out.cols; j++) out.at(i, j) /= sum;
    }
    return wrap(vm, std::move(out));
}
NATIVE(m_sum) {
    Mat& a = M(args);
    Value axis = optArg(args, argc, 0);
    if (axis.isNull()) { double s = 0; for (double x : a.d) s += x; return Value(s); }
    if (axis.isNum() && axis.n == 0) { // suma de cada columna -> 1×cols
        Mat out(1, a.cols);
        for (int i = 0; i < a.rows; i++) for (int j = 0; j < a.cols; j++) out.d[j] += a.at(i, j);
        return wrap(vm, std::move(out));
    }
    if (axis.isNum() && axis.n == 1) { // suma de cada fila -> rows×1
        Mat out(a.rows, 1);
        for (int i = 0; i < a.rows; i++) for (int j = 0; j < a.cols; j++) out.d[i] += a.at(i, j);
        return wrap(vm, std::move(out));
    }
    vm.raise("Matrix.sum: el eje debe ser 0 (columnas) o 1 (filas)");
}
NATIVE(m_mean) {
    UNUSED;
    Mat& a = M(args);
    if (a.d.empty()) return Value(0);
    double s = 0;
    for (double x : a.d) s += x;
    return Value(s / (double)a.d.size());
}
NATIVE(m_max) { UNUSED; auto& d = M(args).d; return d.empty() ? Value() : Value(*std::max_element(d.begin(), d.end())); }
NATIVE(m_min) { UNUSED; auto& d = M(args).d; return d.empty() ? Value() : Value(*std::min_element(d.begin(), d.end())); }
NATIVE(m_argmax) {
    UNUSED;
    Mat& a = M(args);
    ObjList* l = vm.newList();
    for (int i = 0; i < a.rows; i++) {
        int best = 0;
        for (int j = 1; j < a.cols; j++) if (a.at(i, j) > a.at(i, best)) best = j;
        l->items.push_back(Value(best));
    }
    return Value::object(l);
}
NATIVE(m_equals) {
    Value o = optArg(args, argc, 0);
    double tol = argNum(vm, args, argc, 1, "Matrix.equals", 1e-9);
    if (!isMatrix(vm, o)) return Value::boolean(false);
    Mat& a = M(args);
    Mat& b = asMat(vm, o, "Matrix.equals");
    if (a.rows != b.rows || a.cols != b.cols) return Value::boolean(false);
    for (size_t i = 0; i < a.d.size(); i++) if (std::fabs(a.d[i] - b.d[i]) > tol) return Value::boolean(false);
    return Value::boolean(true);
}
NATIVE(m_toString) {
    UNUSED;
    Mat& a = M(args);
    std::string s = "Matrix(" + shape(a) + ")";
    int rmax = std::min(a.rows, 6), cmax = std::min(a.cols, 8);
    for (int i = 0; i < rmax; i++) {
        s += "\n  [";
        for (int j = 0; j < cmax; j++) {
            if (j) s += ", ";
            double x = a.at(i, j);
            s += formatNumber(std::fabs(x) < 1e-12 ? 0 : std::round(x * 10000) / 10000);
        }
        if (a.cols > cmax) s += ", ...";
        s += "]";
    }
    if (a.rows > rmax) s += "\n  ...";
    return vm.str(s);
}

// Estáticos
NATIVE(ms_fromList) { return wrap(vm, fromList(vm, optArg(args, argc, 0), "Matrix.fromList")); }
NATIVE(ms_identity) {
    int n = dim(vm, args, argc, 0, "Matrix.identity");
    Mat m(n, n);
    for (int i = 0; i < n; i++) m.at(i, i) = 1;
    return wrap(vm, std::move(m));
}
NATIVE(ms_random) {
    int r = dim(vm, args, argc, 0, "Matrix.random"), c = dim(vm, args, argc, 1, "Matrix.random");
    double lo = argNum(vm, args, argc, 2, "Matrix.random", -1), hi = argNum(vm, args, argc, 3, "Matrix.random", 1);
    Mat m(r, c);
    for (auto& x : m.d) x = lo + randomUnit() * (hi - lo); // misma semilla que randomSeed()
    return wrap(vm, std::move(m));
}
NATIVE(ms_zeros) { return wrap(vm, Mat(dim(vm, args, argc, 0, "Matrix.zeros"), dim(vm, args, argc, 1, "Matrix.zeros", 1), 0)); }
NATIVE(ms_ones) { return wrap(vm, Mat(dim(vm, args, argc, 0, "Matrix.ones"), dim(vm, args, argc, 1, "Matrix.ones", 1), 1)); }

void registerMatrix(VM& vm) {
    ObjClass* c = vm.nativeClass("Matrix",
        "Matriz de números (filas × columnas) calculada en C++: la base para redes neuronales, estadística y gráficos. "
        "Se opera con +, -, *, / (elemento a elemento o con un número) y dot() para el producto de matrices.",
        c_matrix, "(filas: number|list, columnas?: number = 1, valor?: number = 0)");
    vm.defineClassProp(c, "rows", "number", "Número de filas.", m_rows);
    vm.defineClassProp(c, "cols", "number", "Número de columnas.", m_cols);
    vm.defineClassProp(c, "size", "number", "Número total de valores (filas × columnas).", m_size);
    vm.defineMethod(c, "get", "(fila: number, columna?: number = 0): number", "Valor en (fila, columna).", m_get);
    vm.defineMethod(c, "set", "(fila: number, columna: number, valor: number): void", "Cambia el valor en (fila, columna).", m_set);
    vm.defineMethod(c, "row", "(fila: number): list<number>", "Una fila como lista.", m_row);
    vm.defineMethod(c, "col", "(columna: number): list<number>", "Una columna como lista.", m_col);
    vm.defineMethod(c, "toList", "(): list", "Lista de filas [[...], [...]].", m_toList);
    vm.defineMethod(c, "flatten", "(): list<number>", "Todos los valores en una sola lista, fila tras fila.", m_flatten);
    vm.defineMethod(c, "copy", "(): Matrix", "Copia independiente.", m_copy);
    vm.defineMethod(c, "reshape", "(filas: number, columnas: number): Matrix", "Los mismos valores con otra forma.", m_reshape);
    vm.defineMethod(c, "add", "(otra: Matrix|number): Matrix", "Suma elemento a elemento (también con +).", m_add);
    vm.defineMethod(c, "sub", "(otra: Matrix|number): Matrix", "Resta elemento a elemento (también con -).", m_sub);
    vm.defineMethod(c, "mul", "(otra: Matrix|number): Matrix", "Multiplica elemento a elemento (también con *). Para el producto de matrices usa dot().", m_mul);
    vm.defineMethod(c, "div", "(otra: Matrix|number): Matrix", "Divide elemento a elemento (también con /).", m_div);
    vm.defineMethod(c, "pow", "(exponente: number): Matrix", "Eleva cada valor a un exponente.", m_pow);
    vm.defineMethod(c, "dot", "(otra: Matrix): Matrix", "Producto de matrices (filas por columnas).", m_dot);
    vm.defineMethod(c, "transpose", "(): Matrix", "Transpuesta: las filas pasan a ser columnas.", m_transpose);
    vm.defineMethod(c, "map", "(f: function): Matrix", "Aplica f a cada valor (más lento que las funciones nativas).", m_map);
    vm.defineMethod(c, "sigmoid", "(): Matrix", "Función sigmoide 1 / (1 + e^-x) en cada valor.", m_sigmoid);
    vm.defineMethod(c, "relu", "(): Matrix", "ReLU: max(0, x) en cada valor.", m_relu);
    vm.defineMethod(c, "reluDeriv", "(): Matrix", "Derivada de ReLU: 1 si x > 0, si no 0.", m_reluDeriv);
    vm.defineMethod(c, "tanh", "(): Matrix", "Tangente hiperbólica en cada valor.", m_tanh);
    vm.defineMethod(c, "softmax", "(): Matrix", "Softmax por filas: cada fila pasa a ser una distribución de probabilidad.", m_softmax);
    vm.defineMethod(c, "exp", "(): Matrix", "e^x en cada valor.", m_exp);
    vm.defineMethod(c, "log", "(): Matrix", "Logaritmo natural en cada valor.", m_log);
    vm.defineMethod(c, "abs", "(): Matrix", "Valor absoluto en cada valor.", m_abs);
    vm.defineMethod(c, "sqrt", "(): Matrix", "Raíz cuadrada en cada valor.", m_sqrt);
    vm.defineMethod(c, "clip", "(minimo: number, maximo: number): Matrix", "Limita cada valor al intervalo.", m_clip);
    vm.defineMethod(c, "sum", "(eje?: number): any", "Suma de todo (número), de cada columna (eje 0, fila 1×n) o de cada fila (eje 1, columna n×1).", m_sum);
    vm.defineMethod(c, "mean", "(): number", "Media de todos los valores.", m_mean);
    vm.defineMethod(c, "max", "(): number", "Valor máximo.", m_max);
    vm.defineMethod(c, "min", "(): number", "Valor mínimo.", m_min);
    vm.defineMethod(c, "argmax", "(): list<number>", "Para cada fila, la columna del valor máximo (la clase predicha en clasificación).", m_argmax);
    vm.defineMethod(c, "equals", "(otra: Matrix, tolerancia?: number = 0.000000001): bool", "¿Misma forma y valores (con tolerancia)?", m_equals);
    vm.defineMethod(c, "toString", "(): text", "Descripción con los primeros valores.", m_toString);
    vm.defineStatic(c, "fromList", "(filas: list): Matrix", "Matriz a partir de [[1, 2], [3, 4]] (o una fila [1, 2, 3]).", ms_fromList);
    vm.defineStatic(c, "identity", "(n: number): Matrix", "Matriz identidad n×n.", ms_identity);
    vm.defineStatic(c, "random", "(filas: number, columnas: number, minimo?: number = -1, maximo?: number = 1): Matrix",
                    "Valores aleatorios uniformes (usa la semilla de randomSeed).", ms_random);
    vm.defineStatic(c, "zeros", "(filas: number, columnas?: number = 1): Matrix", "Matriz de ceros.", ms_zeros);
    vm.defineStatic(c, "ones", "(filas: number, columnas?: number = 1): Matrix", "Matriz de unos.", ms_ones);
}

} // namespace hz
