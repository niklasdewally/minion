/* Minion Constraint Solver
   http://minion.sourceforge.net
   
   For Licence Information see file LICENSE.txt 

   $Id$
*/

/* Minion
* Copyright (C) 2006
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "../minion.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>


volatile bool* trig;

StateObj* stateObj;

bool check_double_ctrlc;

void trigger_function(int /* signum */ )
{
  *trig = true;
}

void activate_trigger(volatile bool* b)
{
  trig = b;
  *trig = false;
  
  struct itimerval timer;
          
  signal(SIGALRM, trigger_function);
	timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);
}

void ctrlc_function(int /* signum */ )
{
  if(check_double_ctrlc)
  {
    cerr << "Ctrl+C pressed twice. Exiting immediately." << endl;
    exit(1);
  }
  
  if(trig == NULL)
  {
    cerr << "Search has not started. Exiting immediately." << endl;
    exit(1);
  }
  
  check_double_ctrlc = true;
  
  cerr << "Ctrl+C pressed. Exiting.\n";
  // This is the quickest way to get things to stop.
  *trig = true;
  getState(stateObj).setCtrlcPressed();
}

void install_ctrlc_trigger(void* _stateObj)
{
  check_double_ctrlc = false;
  stateObj = (StateObj*)_stateObj;
  signal(SIGINT, ctrlc_function);
}
