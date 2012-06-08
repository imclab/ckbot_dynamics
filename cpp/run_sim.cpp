/*
* CKBot Kinodynamic Planning with OMPL
* Copyright (C) 2012 Ian O'Hara
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include<eigen3/Eigen/Dense>
#include<vector>
#include<iostream>
#include<fstream>
#define _USE_MATH_DEFINES
#include<math.h>
#include<boost/bind.hpp>
#include<boost/program_options.hpp>
#include<boost/filesystem.hpp>

#include<ompl/control/SimpleSetup.h>
#include<ompl/control/planners/kpiece/KPIECE1.h>
#include<ompl/control/spaces/RealVectorControlSpace.h>
#include<ompl/base/spaces/RealVectorStateSpace.h>
#include<ompl/control/Control.h>

namespace ob = ompl::base;
namespace oc = ompl::control;

#include<json/json.h>

#include "ckbot.hpp"
#include "ck_ompl.hpp"

const char DELIMITER = '/';

bool fill_start_and_goal(const Json::Value& sim_root, 
                         std::vector<double>& s0, 
                         std::vector<double>& s_fin);
bool load_and_run_simulation(std::ostream& out_file, struct sim_settings sets);
bool fill_module(const Json::Value&, ckbot::module_link*);
ckbot::CK_ompl setup_ckbot(Json::Value& chain_root, std::ostream& out_file);
bool save_sol(oc::SimpleSetup& ss, std::ostream& out_file);
bool save_full_tree(oc::SimpleSetup& ss, std::ostream& out_file);

struct sim_settings {
    std::string sim_dir;
    std::string desc_path;
    std::string chain_path;
    std::string sim_path;
    std::string result_dir;
    std::string result_path;

    unsigned int min_control_steps;
    unsigned int max_control_steps;
    double dt;
    double min_torque;
    double max_torque;

    float max_sol_time;

    unsigned int debug;
    bool save_full_tree;
};

int main(int ac, char* av[])
{
    std::string sim_dir("");
    std::string desc_path("description.txt");
    std::string chain_path("chain.txt");
    std::string sim_path("sim.txt");
    std::string result_dir("results/");
    std::string result_path("results.txt");

    struct sim_settings sets = {
        "",
        "description.txt",
        "chain.txt",
        "sim.txt",
        "results/",
        "results.txt",

        1,      /* OMPL min control steps */
        1,      /* OMPL max control steps */
        0.05,   /* OMPL timestep resolution */
        -1.0,   /* Minimum link torque */
        1.0,    /* Max link torque */

        30,     /* Solution search timeout in [s] */
        1,      /* Debugging output? */
        false   /* Save the full planning tree? */
    };

    /* Parse options and execute options */
    namespace po = boost::program_options;
    try 
    {
        po::options_description desc("Usage:");
        desc.add_options()
            ("help", "Give non-sensical help.")
            ("dir", po::value<std::string>(), 
               "Set the directory in which the simulation to run exists")
            ("time", po::value<double>(), "Set the maximum runtime of the solver")
            ("min_control_steps", po::value<unsigned int>(), "Set the minimum number of steps OMPL applies controls for")
            ("max_control_steps", po::value<unsigned int>(), "Set the maximum number of steps OMPL applies controls for")
            ("dt", po::value<double>(), "Set the timestep resolution OMPL uses")
            ("max_torque", po::value<double>(), "Set the maximum torque a link can exert at its joint.")
            ("min_torque", po::value<double>(), "Set the minimum torque a link can exert at its joint.")
            ("tree", "Save the entire planning tree.")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 1;
        }
        if (vm.count("dir")) 
        {
            sets.sim_dir = vm["dir"].as<std::string>();
        }
        if (vm.count("time"))
        {
            sets.max_sol_time = vm["time"].as<double>();
        }
        if (vm.count("dt"))
        {
            sets.dt = vm["dt"].as<double>();
        }
        if (vm.count("min_control_steps"))
        {
            sets.min_control_steps = vm["min_control_steps"].as<unsigned int>();
        }
        if (vm.count("max_control_steps"))
        {
            sets.max_control_steps = vm["max_control_steps"].as<unsigned int>();
        }
        if (vm.count("max_torque"))
        {
            sets.max_torque = vm["max_torque"].as<double>();
        }
        if (vm.count("min_torque"))
        {
            sets.min_torque = vm["min_torque"].as<double>();
        }
        if (vm.count("tree"))
        {
            sets.save_full_tree = true;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Exception of unknown type!" << std::endl;
    }

    /* Check to make sure the simulation directory and all of its components
     * exist
     */
    if (  (sets.sim_dir.length() == 0) 
       || (! boost::filesystem::is_directory(sets.sim_dir)))
    {
        std::cout << "The simululation directory ('" 
                  << sets.sim_dir 
                  << "') specified by --dir must exist!" << std::endl;   
        return 1;
    }
    /* Make sure the directory path ends with a path delimeter */
    if (! (sets.sim_dir.at(sets.sim_dir.length()-1) == DELIMITER))
    {
        sets.sim_dir.push_back(DELIMITER);
    }

    sets.desc_path = sets.sim_dir + sets.desc_path;
    sets.chain_path = sets.sim_dir + sets.chain_path;
    sets.sim_path = sets.sim_dir + sets.sim_path;
    sets.result_dir = sets.sim_dir + sets.result_dir;
    sets.result_path = sets.result_dir + sets.result_path;
    if (! boost::filesystem::is_regular_file(sets.desc_path)) 
    {
        std::cout << "The description file does not exist. (" 
                  << sets.desc_path 
                  << ")" << std::endl;
        return 1;
    }
    if (! boost::filesystem::is_regular_file(sets.chain_path))
    {
        std::cout << "The chain file does not exist. ( " <<
           sets.chain_path << ")" << std::endl;
        return 1;
    }
    if (! boost::filesystem::is_regular_file(sets.sim_path))
    {
        std::cout << "The simulation file does not exist. ( " <<
           sets.sim_path << ")" << std::endl;
        return 1;
    }

    /* Verify and (if needed) create the result directory */
    if (! boost::filesystem::is_directory(sets.result_dir))
    {
        std::cout << "The result directory doesn't exist yet...creating...";
        try 
        {
            boost::filesystem::create_directory(sets.result_dir);
            std::cout << "Last char is: " 
                      << sets.sim_dir.at(sets.sim_dir.length()-1) << std::endl;
        }
        catch (std::exception& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error occured while creating directory" << std::endl;
        }
        std::cout << "Success!" << std::endl;
    }

    bool sim_status = load_and_run_simulation(std::cout, sets);
    if (! sim_status)
    {
        std::cout << "Error during simulation..exiting." << std::endl;
        return 1;
    }
    return 0;
}

/*
 * Take a json chain subtree, which is an array of module dictionaries,
 * and turn it into a usable chain object populated by the module defined
 * in the module dictionaries.
 */
ckbot::CK_ompl 
setup_ckbot(Json::Value& chain_root, std::ostream& out_file=std::cout)
{
    Json::Value chain_array = chain_root["chain"];
    int num_modules = chain_array.size();
    /* This will never be explicitly freed, just let program end do it. */
    ckbot::module_link* modules_for_chain = new ckbot::module_link[num_modules];
    for (unsigned int link=0; link<num_modules; ++link)
    {
        
        out_file << " Attempting to fill chain link " << link << " of " << num_modules << std::endl;
        if (! fill_module(chain_array[link], &modules_for_chain[link]))
        {
            throw "Error";
        }
    }
    /* Again, never explictly freed.  Let the program run till death! */
    ckbot::chain *ch = new ckbot::chain(modules_for_chain, num_modules);
    ckbot::CK_ompl rate_machine(*ch);
    return rate_machine;
}

/*
 * Load a simulation from files containing json descriptions of the chain, 
 * and start and goal positions.
 * Then run and save the results for later. 
 */
bool
load_and_run_simulation(std::ostream& out_file, struct sim_settings sets)
{
    /*****
    * Load both the CKBot chain simulator and the start and end goals
    * While loading the chain description, start outputing the description
    * to the result file.  Using only one file for the results guarantees
    * nothing gets mixed up (ie: One file describes it all).
    *****/
    std::ifstream chain_file;
    std::ifstream sim_file;
    std::ofstream result_file;
    Json::Value chain_root;
    Json::Reader chain_reader;
    Json::Value sim_root;
    Json::Reader sim_reader;
    
    /* No reason to run if we can't open our result file, 
     * so let the exceptions flow up.
     */
    result_file.open((char*)sets.result_path.c_str());
    result_file << "{" << std::endl;

    chain_file.open((char*)sets.chain_path.c_str());
    bool parsingSuccessful = chain_reader.parse(chain_file, chain_root);
    chain_file.close();
    if (!parsingSuccessful)
    {
        std::cerr << "Couldn't parse chain file." << std::endl;
        return false;
    }
   
    /*
     * Load the chain and initialize an ODE solver for it from the JSON tree
     * we now have. Also, while we're at it, output the chain description
     * to the result file.
     */
    ckbot::CK_ompl rate_machine = setup_ckbot(chain_root);
    rate_machine.get_chain().describe_self(result_file);
    result_file << "," << std::endl;
    
    /* For reference when setting up OMPL */
    int num_modules = rate_machine.get_chain().num_links();
    ckbot::module_link first_module = rate_machine.get_chain().get_link(0u);

    /* The start and goal positions are in a separate file. 
     * Hopefully this allows easy re-use of config parts
     */
    sim_file.open((char*)sets.sim_path.c_str());
    bool sim_parse_success = sim_reader.parse(sim_file, sim_root);
    sim_file.close();

    if (!sim_parse_success)
    {
        std::cerr << "Couldn't parse sim file." << std::endl;
        return false;
    }

    /* Fill the start and goal positions from the sim JSON tree */
    std::vector<double> s0(2*num_modules);
    std::vector<double> s_fin(2*num_modules);
    fill_start_and_goal(sim_root, s0, s_fin);

    /*****
     * Setup OMPL 
     *****/
    /* Make our configuration space and set the bounds on each module's angles */
    ob::StateSpacePtr space(new ob::RealVectorStateSpace(2*num_modules));
    space->as<ob::RealVectorStateSpace>()->setBounds(first_module.get_joint_min(), 
                                                             first_module.get_joint_max());
    /* Make our control space, which is one bound direction for each joint (Torques) */
    oc::ControlSpacePtr cspace(new oc::RealVectorControlSpace(space, num_modules));
    ob::RealVectorBounds cbounds(num_modules);
    cbounds.setLow(sets.min_torque);
    cbounds.setHigh(sets.max_torque);
    cspace->as<oc::RealVectorControlSpace>()->setBounds(cbounds); // TODO (IMO): Arbitrary for now

    /* 
     * Use OMPL's built in setup mechanism instead of allocating state space information
     * and problem defintion pointers on my own
     */
    oc::SimpleSetup ss(cspace);

    // TODO: Write an actual state validity checker!
    ss.setStateValidityChecker(boost::bind<bool>(&ckbot::CK_ompl::stateValidityChecker, 
                                                 rate_machine, 
                                                 _1));

    /* Setup and get the dynamics of our system into the planner */
    oc::ODEBasicSolver<> odeSolver(ss.getSpaceInformation(), 
                                              boost::bind<void>(&ckbot::CK_ompl::CKBotODE, 
                                                                rate_machine, _1, _2, _3));
    ss.setStatePropagator(odeSolver.getStatePropagator());

    /* Define the start and end configurations */
    ob::ScopedState<ob::RealVectorStateSpace> start(ss.getSpaceInformation());
    ob::ScopedState<ob::RealVectorStateSpace> goal(ss.getSpaceInformation());
    for (int i = 0; i < 2*num_modules; ++i)
    {
        start[i] = s0[i];
        goal[i] = s_fin[i];
    }
    start.print(std::cout);
    goal.print(std::cout);

    ss.setStartAndGoalStates(start, goal);

    ob::PlannerPtr planner(new oc::KPIECE1(ss.getSpaceInformation()));
    ss.setPlanner(planner);
    /* Allow this range of number of steps in our solution */
    ss.getSpaceInformation()->setMinMaxControlDuration(sets.min_control_steps, sets.max_control_steps);
    ss.getSpaceInformation()->setPropagationStepSize(sets.dt);
    
    /* Tell SimpleSetup that we've given it all of the info, and that
     * it should distribute parameters to the different components 
     * (mostly, fill in the planner parameters.) 
     */
    ss.setup();

    /* Debug printing section for information having to do with the setup
     * of the planner and any of its components.
     * This should be the only debug printing section for this purpose.
     */
    if (sets.debug)
    {
        /* Have the chain in our rate machine describe itself */
        rate_machine.get_chain().describe_self(out_file);

        /* Print the planner start and goal states */
        out_file << "Planner Start and Goal states: " << std::endl;
        const ob::ProblemDefinitionPtr pProbDef = planner->getProblemDefinition();
        pProbDef->print(out_file);

        /* Print the current planner parameter list */
        std::map< std::string, std::string > params;
        planner->params().getParams(params); 

        out_file << "Planner Parameter list: " << std::endl; 
        std::map< std::string, std::string >::iterator paramIt;
        for (paramIt = params.begin(); paramIt != params.end(); paramIt++)
        {
            out_file << "    " << paramIt->first << ", " << paramIt->second << std::endl;
        }

        /* Print the control space bounds */
        const oc::ControlSpacePtr pControl = ss.getControlSpace();
        const ob::RealVectorBounds bounds = pControl->as<oc::RealVectorControlSpace>()->getBounds();
        std::vector<double> low = bounds.low;
        std::vector<double> high = bounds.high;
        std::vector<double>::iterator l_it;
        std::vector<double>::iterator h_it;

        out_file << "Control Space bounds: " << std::endl; 
        unsigned int counter = 0;
        for (l_it = low.begin(), h_it = high.begin(); 
            (l_it != low.end()) && (h_it != high.end()); 
            l_it++, h_it++)
        {
            out_file << "    Link " << ++counter 
                << ": Low: " << *l_it 
                << " High: " << *h_it << std::endl;
        }

        /* Print the configuration space bounds */    
        const ob::RealVectorBounds cbounds = space->as<ob::RealVectorStateSpace>()->getBounds();
        out_file << "The Configuration bounds are: " << std::endl;
        std::vector<double> clow = cbounds.low;
        std::vector<double> chigh = cbounds.high;
        std::vector<double>::iterator bl_it;
        std::vector<double>::iterator bh_it;
        unsigned int config_counter = 0;
        for (bl_it = clow.begin(), bh_it = chigh.begin(); 
            (bl_it != clow.end()) && (bh_it != chigh.end()); 
             bl_it++,bh_it++)
        {
            out_file << "    Dof" << ++config_counter 
                << ": Low: " << *bl_it 
                << " High: " << *bh_it << std::endl; 
        }

        /* Print information about how the planner will interface with the dynamics engine */
        out_file << "The progagationStepSize is: " 
            << ss.getSpaceInformation()->getPropagationStepSize()
            << std::endl << "    The Minimum number of steps is: " 
            << ss.getSpaceInformation()->getMinControlDuration() << std::endl 
            << "    The Max number of steps is: " 
            << ss.getSpaceInformation()->getMaxControlDuration() << std::endl;

        /* Print information about the ODE Solver */
        out_file << "The ODE Step size is: " << odeSolver.getIntegrationStepSize() << std::endl;
    }
    
    bool solve_status = false;
    if (ss.solve(sets.max_sol_time))
    {
        solve_status = true;
        save_sol(ss, result_file);
        if (sets.save_full_tree)
        {
            save_full_tree(ss, out_file); /* DEBUG: replace out_file with result_file after this works */
        }
    } 

    result_file << "}" << std::endl;
    result_file.close();   
    return solve_status;
}


/* 
 * Output solution to file in json format
 */
bool 
save_sol(oc::SimpleSetup& ss, std::ostream& out_file)
{
    const oc::PathControl& sol_path(ss.getSolutionPath());
    unsigned int num_modules = (*(ss.getStateSpace())).as<ob::RealVectorStateSpace>()->getDimension()/2;
    std::vector<double> time(sol_path.getStateCount());
    std::vector<double> dt(sol_path.getStateCount()-1);

    /* Note: We're assuming that we're already in a dictionary */
    out_file << "\"control\": [" << std::endl;   

    time[0] = 0.0;
    for (unsigned int i=0; i < sol_path.getStateCount(); ++i)
    {
        if (i != 0)
        {
            out_file << "{" << std::endl;
            const ob::RealVectorStateSpace::StateType& s_minus = *sol_path.getState(i-1)->as<ob::RealVectorStateSpace::StateType>();
            const ob::RealVectorStateSpace::StateType& s = *sol_path.getState(i)->as<ob::RealVectorStateSpace::StateType>();

            dt[i-1] = sol_path.getControlDuration(i-1); // Control Duration to go from state i-1 to i;
            time[i] = time[i-1]+dt[i-1]; // Time at this step
            out_file << "\"start_state_index\":" << i-1 << "," << std::endl;
            out_file << "\"end_state_index\":" << i << "," << std::endl;

            out_file << "\"start_state\": [";
            for (unsigned int j=0; j<2*num_modules; ++j)
            {
                out_file << s_minus[j];
                if (j+1 < 2*num_modules)
                {
                    out_file << ", ";
                }
            }
            out_file << "]," << std::endl;

            out_file << "\"end_state\": [";
            for (unsigned int j=0; j<2*num_modules; ++j)
            {
                out_file  << s[j];
                if (j+1 < 2*num_modules)
                {
                    out_file << ", ";
                }
            }
            out_file << "]," << std::endl;
            
            out_file << "\"start_time\":" << time[i-1] << "," << std::endl;
            out_file << "\"end_time\":" << time[i] << "," << std::endl;
            out_file << "\"dt\":" << dt[i-1] << "," << std::endl;
            const double* c = sol_path.getControl(i-1)->as<oc::RealVectorControlSpace::ControlType>()->values;
            out_file << "\"control\": [";
            for (unsigned int j=0; j<num_modules; ++j)
            {
                out_file << " " << c[j];
                if (j+1 < num_modules)
                {
                    out_file << ", ";
                }
            }
            out_file << "]";

            /* End this control dictionary and prepare for another, if needed */
            out_file << "}";
            if (i+1 < sol_path.getStateCount())
            {
                out_file << ",";
            }
            out_file << std::endl;
        }
    }
    out_file << "]" << std::endl;
    return true;
}

bool
save_full_tree(oc::SimpleSetup& ss, std::ostream& out_file)
{
    oc::KPIECE1 *kPlanner = ss.getPlanner()->as<oc::KPIECE1>();
    oc::PlannerData data;
    kPlanner->getPlannerData(data);
    data.print(out_file);
    /* Wow, that was easy... */
}

bool
fill_module(const Json::Value& json_mod, ckbot::module_link* module)
{
    ckbot::module_description this_module_desc;
    double damping = json_mod["damping"].asDouble();
    double mass = json_mod["mass"].asDouble();
    double joint_max = json_mod["joint_max"].asDouble();
    double joint_min = json_mod["joint_min"].asDouble();
    double torque_max = json_mod["torque_max"].asDouble();

    Eigen::Vector3d forward_joint_axis;
    Eigen::Vector3d r_ip1; 
    Eigen::Vector3d r_im1;
    if ((json_mod["f_jt_axis"].size() != 3)
            || (json_mod["r_im1"].size() != 3)
            || (json_mod["r_ip1"].size() != 3))
    {
        return false;
    }
    for (unsigned int m=0; m<3; ++m)
    {
        /*Need 0u as index to distinguish from operator[] which takes a string*/
        forward_joint_axis(m) = ((json_mod["f_jt_axis"])[m])[0u].asDouble();
        r_im1(m) = ((json_mod["r_im1"])[m])[0u].asDouble();
        r_ip1(m) = ((json_mod["r_ip1"])[m])[0u].asDouble();
    } 

    Eigen::Matrix3d I_cm;
    Eigen::Matrix3d R_jts;
    Eigen::Matrix3d init_rotation;
    /* TODO: Check that the json for these 3 are 3x3 arrays!!! */ 
    for (unsigned int m=0; m < 3; ++m)
    {
        for (unsigned int n=0; n<3; ++n)
        {
            I_cm(m,n) = json_mod["I_cm"][m][n].asDouble();
            R_jts(m,n) = json_mod["R_jts"][m][n].asDouble();
            init_rotation(m,n) = json_mod["init_rotation"][m][n].asDouble();
        }
    }

    this_module_desc.damping = damping;
    this_module_desc.m = mass;
    this_module_desc.joint_max = joint_max;
    this_module_desc.joint_min = joint_min;
    this_module_desc.torque_max = torque_max;
    this_module_desc.forward_joint_axis = forward_joint_axis;
    this_module_desc.r_im1 = r_im1;
    this_module_desc.r_ip1 = r_ip1;
    this_module_desc.I_cm = I_cm;
    this_module_desc.R_jts = R_jts;
    this_module_desc.init_rotation = init_rotation;

    ckbot::module_link* this_module = new ckbot::module_link(this_module_desc);
    /* TODO: This doesn't really make sense.  Should make module pointer point to the newly allocated memory, not copy the newly allocated memory */
    *module = *this_module;

    return true;
}

bool
fill_start_and_goal(const Json::Value& sim_root, 
                    std::vector<double>& s0, 
                    std::vector<double>& s_fin)
{
    if((! sim_root["start"].isArray()) || 
       (! sim_root["goal"].isArray()))
    {
        std::cerr << "In simulation file the start and " <<
                     "goal positions must be Json arrays." << std::endl;
        return false;
    }
    if ((sim_root["start"].size() != s0.size()) || 
        (sim_root["goal"].size() != s_fin.size()))
    {
        std::cerr << "The start and goal positions in the sim file " << 
                     "must have the same dimension as the chain " <<
                     "suggests (ie: #modules*2)." << std::endl;
        return false;
    }

    for (unsigned int i = 0; i < s0.size(); ++i)
    {
       s0[i] = sim_root["start"][i].asDouble();
       s_fin[i] = sim_root["goal"][i].asDouble();
    }
    return true;
}
