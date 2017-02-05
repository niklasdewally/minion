/*
* Minion http://minion.sourceforge.net
* Copyright (C) 2006-09
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#include "variable_orderings.h"
#include "common_search.h"
#include "../preprocess.h"

namespace Controller {

template <typename T>
void inline maybe_print_search_assignment(T& var, DomainInt val, BOOL equal) {
  if(getOptions().dumptree) {
    cout << "SearchAssign:" << var << (equal ? " = " : " != ") << val << endl;
  }
  if(getOptions().dumpjsontree.isActive()) {
    if(equal) {
      getOptions().dumpjsontree.mapElement("branchVar", tostring(var));
      getOptions().dumpjsontree.mapElement("branchVal", val);
      getOptions().dumpjsontree.openMapWithKey("left");
    }
    else {
          getOptions().dumpjsontree.openMapWithKey("right");
    }
  }
}

// instead of carrying around the pos everywhere, the VariableOrder object has
// pos in it as a reversible<SysInt>.

// replace find_next_unassigned with all_vars_assigned (which maybe uses
// something like a watch).

// remove template on branchtype. make virtual.

struct SearchManager {

  virtual ~SearchManager() {}

  vector<AnyVarRef> var_array;
  shared_ptr<VariableOrder> var_order;

  bool hasauxvars;  // Has a VARORDER AUX
  SysInt topauxvar; // lowest index of an aux var.

  shared_ptr<Propagate> prop; // Propagate is the type of the base class. Method
                              // prop->prop(var_array)

  vector<Controller::triple> branches; // L & R branches so far (isLeftBranch?,var,value)

  SysInt depth; // number of left branches

  SearchManager(vector<AnyVarRef> _var_array, vector<SearchOrder> _order,
                shared_ptr<VariableOrder> _var_order, shared_ptr<Propagate> _prop)
      : var_array(_var_array), var_order(_var_order), topauxvar(0), prop(_prop), depth(0) {
    branches.reserve(var_array.size());
    hasauxvars = _order.back().find_one_assignment;
    if(hasauxvars) {
      for(SysInt i = 0; i < (SysInt)_order.size() - 1; i++) {
        topauxvar += _order[i].var_order.size();
      }
    }
  }

  void reset() {
    branches.clear();
    depth = 0;
  }

  // Returns true if all variables assigned
  inline bool all_vars_assigned() {
    pair<SysInt, DomainInt> picked = var_order->pickVarVal();
    return picked.first == -1;
  }

  SysInt search_depth() {
    return depth;
  }

  // returns false if left branch not possible.
  inline void branch_left(pair<SysInt, DomainInt> picked) {
    D_ASSERT(picked.first != -1);
    D_ASSERT(!var_array[picked.first].isAssigned());

    world_push();
    var_array[picked.first].assign(picked.second);
    maybe_print_search_assignment(var_array[picked.first], picked.second, true);
    branches.push_back(Controller::triple(true, picked.first, picked.second));
    depth++;
  }

  inline bool branch_right() {
    while(!branches.empty() && !branches.back().isLeft) { // pop off all the
                                                          // RBs
      maybe_print_right_backtrack();
      branches.pop_back();
    }

    if(branches.empty()) {
      return false;
    }

    SysInt var = branches.back().var;
    DomainInt val = branches.back().val;

    // remove the left branch.
    branches.pop_back();
    world_pop();
    depth--;

    D_ASSERT(var_array[var].inDomain(val));

    // special case the upper and lower bounds to make it work for bound
    // variables
    if(var_array[var].getMin() == val) {
      var_array[var].setMin(val + 1);
    } else if(var_array[var].getMax() == val) {
      var_array[var].setMax(val - 1);
    } else {
      var_array[var].removeFromDomain(val);
    }
    maybe_print_search_assignment(var_array[var], val, false);
    branches.push_back(Controller::triple(false, var, val));
    return true;
  }

  inline void jump_out_aux_vars() {
    while(!branches.empty() && branches.back().var >= topauxvar) {
      if(branches.back().isLeft) {
        world_pop();
        maybe_print_right_backtrack();
        depth--;
      }

      branches.pop_back();
    }
  }

  // Steal work isn't used, but is left here in case anyone ever wants it.
  option<std::vector<Controller::triple>>
  steal_work() { // steal the topmost left branch from this search.

    for(UnsignedSysInt newceil = 0; newceil < branches.size(); ++newceil) {
      if(branches[newceil].isLeft) {
        std::vector<Controller::triple> work(branches.begin(), branches.begin() + newceil + 1);
        work.back().isLeft = false;
        branches[newceil].isLeft = false;

        return work;
      }
    }

    return option<std::vector<Controller::triple>>();
  }

  // Most basic search procedure
  virtual void search() {
    maybe_print_node(var_array);
    while(true) {
      D_ASSERT(getQueue().isQueuesEmpty());

      getState().incrementNodeCount();

      do_checks(var_array, branches);

      pair<SysInt, DomainInt> varval = var_order->pickVarVal();

      if(varval.first == -1) {
        // We have found a solution!
        check_sol_is_correct();
        deal_with_solution();
        if(hasauxvars) { // There are AUX vars at the end of the var ordering.
          // Backtrack out of them.
          jump_out_aux_vars();
        }

        // If we are not finished, then go into the loop below.
        getState().setFailed(true);
      } else {
        maybe_print_node(var_array);
        branch_left(varval);
        prop->prop(var_array);
      }

      // loop to
      while(getState().isFailed()) {
        getState().setFailed(false);
        if(depth == 0) {
          return;
        }

        maybe_print_backtrack();
        bool flag = branch_right();
        if(!flag) { // No remaining left branches to branch right.
          return;
        }
        // Deal with optimisation variables 
        if(getState().isOptimisationProblem()) {
          getState().getOptimiseVar()->setMin(getState().getOptimiseValue());
        }
        prop->prop(var_array);
      }
    }
  }
};
}

#endif
