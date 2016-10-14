#include <algorithm>
#include <iostream>
#include <sstream>

#include <coli/debug.h>
#include <coli/core.h>
#include <coli/type.h>
#include <coli/expr.h>
#include <Halide.h>

using namespace Halide;
using namespace Halide::Internal;

using std::string;
using std::map;
using std::set;
using std::vector;

namespace coli
{

coli::primitive_t halide_type_to_coli_type(Type type)
{
    if (type.is_uint()) {
        if (type.bits() == 8) {
            return coli::p_uint8;
        } else if (type.bits() == 16) {
            return coli::p_uint16;
        } else if (type.bits() == 32) {
            return coli::p_uint32;
        } else {
            return coli::p_uint64;
        }
    } else if (type.is_int()) {
        if (type.bits() == 8) {
            return coli::p_int8;
        } else if (type.bits() == 16) {
            return coli::p_int16;
        } else if (type.bits() == 32) {
            return coli::p_int32;
        } else {
            return coli::p_int64;
        }
    } else if (type.is_float()) {
        if (type.bits() == 32) {
            return coli::p_float32;
        } else if (type.bits() == 64) {
            return coli::p_float64;
        } else {
            coli::error("Floats other than 32 and 64 bits are not suppored in Coli.", true);
        }
    } else if (type.is_bool()) {
        return coli::p_boolean;
    } else {
        coli::error("Halide type cannot be translated to Coli type.", true);
    }
    return coli::p_none;
}

namespace
{

template<typename T>
std::string to_string(const std::vector<T>& v) {
    std::ostringstream stream;
    stream << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        stream << v[i];
        if (i != v.size() - 1) {
            stream << ", ";
        }
    }
    stream << "]";
    return stream.str();
}

class HalideToColi : public IRVisitor {
private:
    const vector<Function> &outputs;
    const map<string, Function> &env;
    const map<string, coli::buffer> &output_buffers;
    set<string> seen_lets;
    set<string> seen_buffer;
    map<string, coli::buffer> temporary_buffers;

    void error() {
        coli::error("Can't convert to coli expr.", true);
    }

    void push_loop_dim(const For *op) {
        loop_dims.push_back({op->name, op->min, op->extent});
    }

    void pop_loop_dim() {
        loop_dims.pop_back();
    }

public:
    coli::expr expr;
    map<string, coli::computation> computation_list;
    coli::function &func; // Represent one Halide pipeline

    struct Loop {
        std::string name;
        Expr min, extent;
    };

    vector<Loop> loop_dims;

    HalideToColi(const vector<Function> &outputs, const map<string, Function> &env,
                 const map<string, coli::buffer> &output_buffers, coli::function &f)
        : outputs(outputs), env(env), output_buffers(output_buffers), func(f) {}

    coli::expr mutate(Expr e) {
        assert(e.defined() && "HalideToColi can't convert undefined expr\n");
        // For now, substitute in all lets to make life easier (does not substitute in lets in stmt though)
        e = substitute_in_all_lets(e);
        e.accept(this);
        return expr;
    }

    void mutate(Stmt s) {
        assert(s.defined() && "HalideToColi can't convert undefined stmt\n");
        // For now, substitute in all lets to make life easier (does not substitute in lets in stmt though)
        s = substitute_in_all_lets(s);
        s.accept(this);
    }

protected:
    void visit(const IntImm *);
    void visit(const UIntImm *);
    void visit(const FloatImm *);
    void visit(const Cast *);
    void visit(const Variable *);
    void visit(const Add *);
    void visit(const Sub *);
    void visit(const Mul *);
    void visit(const Div *);
    void visit(const Mod *);
    void visit(const Min *);
    void visit(const Max *);
    void visit(const EQ *);
    void visit(const NE *);
    void visit(const LT *);
    void visit(const LE *);
    void visit(const GT *);
    void visit(const GE *);
    void visit(const And *);
    void visit(const Or *);
    void visit(const Not *);
    void visit(const Select *);

    void visit(const StringImm *)           { error(); }
    void visit(const AssertStmt *)          { error(); }
    void visit(const Evaluate *)            { error(); }
    void visit(const Ramp *)                { error(); }
    void visit(const Broadcast *)           { error(); }
    void visit(const IfThenElse *)          { error(); }
    void visit(const Free *)                { error(); }

    void visit(const Let *);
    void visit(const LetStmt *);
    void visit(const For *);
    void visit(const Load *);
    void visit(const Store *);
    void visit(const Call *);
    void visit(const ProducerConsumer *);
    void visit(const Block *);
    void visit(const Allocate *);

    void visit(const Provide *);
    void visit(const Realize *);
};

void HalideToColi::visit(const IntImm *op) {
    if (op->type.bits() == 8) {
        expr = coli::expr((int8_t)op->value);
    } else if (op->type.bits() == 16) {
        expr = coli::expr((int16_t)op->value);
    } else if (op->type.bits() == 32) {
        expr = coli::expr((int32_t)op->value);
    } else {
        // 64-bit signed integer
        expr = coli::expr(op->value);
    }
}

void HalideToColi::visit(const UIntImm *op) {
    if (op->type.bits() == 8) {
        expr = coli::expr((uint8_t)op->value);
    } else if (op->type.bits() == 16) {
        expr = coli::expr((uint16_t)op->value);
    } else if (op->type.bits() == 32) {
        expr = coli::expr((uint32_t)op->value);
    } else {
        // 64-bit unsigned integer
        expr = coli::expr(op->value);
    }
}

void HalideToColi::visit(const FloatImm *op) {
    if (op->type.bits() == 32) {
        expr = coli::expr((float)op->value);
    } else if (op->type.bits() == 64) {
        expr = coli::expr(op->value);
    } else {
        // Only support 32- and 64-bit integer
        error();
    }
}

void HalideToColi::visit(const Cast *op) {
    error();
}

void HalideToColi::visit(const Variable *op) {
    //TODO(psuriana)
    error();
    const auto &iter = computation_list.find(op->name);
    if (iter != computation_list.end()) {
        // It is a reference to variable defined in Let/LetStmt or a reference
        // to a buffer
        expr = iter->second(0);
    } else {
        // It is presumably a reference to loop variable
        expr = coli::idx(op->name);
    }
}

void HalideToColi::visit(const Add *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = a + b;
}

void HalideToColi::visit(const Sub *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = a - b;
}

void HalideToColi::visit(const Mul *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = a * b;
}

void HalideToColi::visit(const Div *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = a / b;
}

void HalideToColi::visit(const Mod *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = a % b;
}

void HalideToColi::visit(const Min *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = coli::expr(coli::o_min, a, b);
}

void HalideToColi::visit(const Max *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = coli::expr(coli::o_max, a, b);
}

void HalideToColi::visit(const EQ *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a == b);
}

void HalideToColi::visit(const NE *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a != b);
}

void HalideToColi::visit(const LT *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a < b);
}

void HalideToColi::visit(const LE *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a <= b);
}

void HalideToColi::visit(const GT *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a > b);
}

void HalideToColi::visit(const GE *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a >= b);
}

void HalideToColi::visit(const And *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a && b);
}

void HalideToColi::visit(const Or *op) {
    coli::expr a = mutate(op->a);
    coli::expr b = mutate(op->b);
    expr = (a || b);
}

void HalideToColi::visit(const Not *op) {
    coli::expr a = mutate(op->a);
    expr = !a;
}

void HalideToColi::visit(const Select *op) {
    coli::expr cond = mutate(op->condition);
    coli::expr t = mutate(op->true_value);
    coli::expr f = mutate(op->false_value);
    expr = coli::expr(coli::o_cond, cond, t, f);
}

void HalideToColi::visit(const Let *op) {
    assert(is_const(op->value) && "Only support let of constant for now.\n");
    assert((seen_lets.find(op->name) != seen_lets.end()) && "Redefinition of lets is not supported right now.\n");
    seen_lets.insert(op->name);


    coli::expr value = mutate(op->value);
    //TODO(psuriana): potential segfault here since we're passing stack pointer
    coli::constant c_const(op->name, &value, halide_type_to_coli_type(op->value.type()), true, NULL, 0, &func);
    computation_list.emplace(op->name, c_const);

    coli::expr body = mutate(op->body);
    expr = body;
}

void HalideToColi::visit(const LetStmt *op) {
    assert(is_const(op->value) && "Only support let of constant for now.\n");
    assert((seen_lets.find(op->name) != seen_lets.end()) && "Redefinition of lets is not supported right now.\n");
    seen_lets.insert(op->name);

    coli::expr value = mutate(op->value);
    //TODO(psuriana): potential segfault here since we're passing stack pointer
    coli::constant c_const(op->name, &value, halide_type_to_coli_type(op->value.type()), true, NULL, 0, &func);
    computation_list.emplace(op->name, c_const);

    mutate(op->body);
}

void HalideToColi::visit(const ProducerConsumer *op) {
    assert((op->body.as<Block>() == NULL) && "Does not currently handle update.\n");
    assert((computation_list.count(op->name) == 0) && "Find another computation with the same name.\n");

    //TODO(psuriana): ideally should create the computation here, but what to pass as iter dom and expr?

    vector<Loop> old_loop_dims = loop_dims;
    mutate(op->body);
    loop_dims = old_loop_dims;
}

void HalideToColi::visit(const For *op) {
    push_loop_dim(op);
    mutate(op->body);
    pop_loop_dim();
}

void HalideToColi::visit(const Load *op) {
    //TODO(psuriana): doesn't handle load to image or some external buffer right now
    error();
}

void HalideToColi::visit(const Provide *op) {
    //TODO(psuriana): depending on which lowering stage we're passing, this might still exist in the IR
    assert((computation_list.count(op->name) == 0) && "Find another computation with the same name.\n");
    assert((temporary_buffers.count("buff_" + op->name) || output_buffers.count("buff_" + op->name))
           && "The buffer should have been allocated previously.\n");

    vector<coli::expr> args(op->args.size());
    for (size_t i = 0; i < op->args.size(); ++i) {
        assert((op->args[i].as<Variable>() != NULL) && "Expect args of provide to be loop index for now.\n");
        args[i] = mutate(op->args[i]);
    }

    assert((op->values.size() == 1) && "Expect 1D store in the Provide node for now.\n");
    vector<coli::expr> values(op->values.size());
    for (size_t i = 0; i < op->values.size(); ++i) {
        values[i] = mutate(op->values[i]);
    }

    string dims_str = to_string(op->args);
    //TODO(psuriana): determine the loop bound
    string iter_space_str = "[N]->{" + op->name + dims_str + ": 0<=i<N and 0<=j<N}";
    //TODO(psuriana): potential segfault here since we're passing stack pointer
    coli::computation compute(iter_space_str, &values[0], false, halide_type_to_coli_type(op->values[0].type()), &func);

    // Map to buffer
    string access_str = "{" + op->name + dims_str + "->" + "buff_" + op->name + dims_str + "}";
    compute.set_access(access_str);

    computation_list.emplace(op->name, compute);
}

void HalideToColi::visit(const Realize *op) {
    //TODO(psuriana): depending on which lowering stage we're passing, this might still exist in the IR
    error();
}

void HalideToColi::visit(const Store *op) {
    //TODO(psuriana): not sure if COLi expect things in 1D???
    error();
}

void HalideToColi::visit(const Call *op) {
    assert((op->call_type == Call::CallType::Halide) && "Only handle call to halide func for now.\n");

    const auto iter = computation_list.find(op->name);
    assert(iter != computation_list.end() && "Computation does not exist.\n");

    vector<coli::expr> args(op->args.size());
    for (size_t i = 0; i < op->args.size(); ++i) {
        args[i] = mutate(op->args[i]);
    }
    expr = iter->second(args);
}

void HalideToColi::visit(const Block *op) {
    mutate(op->first);
    mutate(op->rest);
}

void HalideToColi::visit(const Allocate *op) {
    //TODO(psuriana): how do you express duplicate Allocate (e.g. compute_at at different func defs)?
    assert((temporary_buffers.count("buff_" + op->name) == 0) && "Find duplicate temporary buffer allocation.\n");

    const auto iter = env.find(op->name);
    assert((iter != env.end()) && "Cannot find function in env.\n");
    bool is_output = false;
    for (Function o : outputs) {
        is_output |= o.same_as(iter->second);
    }
    assert(!is_output && "Allocate should have been temporary buffer.\n");

    // Create temporary buffer if it's not an output
    vector<coli::expr> extents(op->extents.size());
    for (size_t i = 0; i < op->extents.size(); ++i) {
        extents[i] = mutate(op->extents[i]);
    }

    string buffer_name = "buff_" + op->name;
    coli::buffer produce_buffer(
        buffer_name, extents.size(), extents,
        halide_type_to_coli_type(op->type), NULL, a_temporary, &func);
    temporary_buffers.emplace(buffer_name, std::move(produce_buffer));
}

} // anonymous namespace

}
