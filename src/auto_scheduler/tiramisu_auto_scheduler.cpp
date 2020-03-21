#include <tiramisu/auto_scheduler/auto_scheduler.h>
#include <tiramisu/auto_scheduler/evaluator.h>
#include <tiramisu/auto_scheduler/search_method.h>

#include <chrono>

namespace tiramisu::auto_scheduler
{

auto_scheduler::auto_scheduler(search_method *searcher, evaluator *eval_func, 
                               tiramisu::function *fct)
                               
    : fct(fct), ast(fct), searcher(searcher), eval_func(eval_func)
{
    searcher->set_eval_func(eval_func);
}

void auto_scheduler::find_schedule()
{
    float initial_exec_time = exec_evaluator->evaluate(ast);
    
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    ast.evaluation = eval_func->evaluate(ast);
    searcher->search(ast);
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    
    std::cout << "NB explored schedules : " << searcher->get_nb_explored_schedules() << std::endl;
    std::cout << "Best evaluation : " << searcher->get_best_evaluation() << std::endl;
    std::cout << "Initial exec time : " << initial_exec_time << std::endl;
    std::cout << "Search time : " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms " << std::endl;  
}

void auto_scheduler::apply_best_schedule()
{
    syntax_tree *best_ast = searcher->get_best_ast();
    best_ast->print_ast();
    
    float best_sched_exec_time = exec_evaluator->evaluate(*best_ast);
    std::cout << "Best schedule exec time : " << best_sched_exec_time << std::endl;
}

}
