/*************************************************************************
 * PLEASE SEE THE FILE "COPYING" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/

/*************************************************************************
 *
 *  file:  semantic_memory.h
 *
 * =======================================================================
 */

#ifndef SEMANTIC_MEMORY_H
#define SEMANTIC_MEMORY_H

#include "kernel.h"

#include "stl_typedefs.h"
#include "smem_structs.h"
#include "smem_settings.h"
#include "smem_stats.h"

#include <string>

//#define SMEM_EXPERIMENT  // hijack the main SMem function for tight-loop experimentation/timing

class SMem_Manager
{
        friend cli::CommandLineInterface;
        friend smem_statement_container;
        friend smem_path_param;
        friend class smem_param_container;
        friend smem_db_lib_version_stat;
        friend smem_mem_usage_stat;
        friend smem_mem_high_stat;
        friend smem_timer_level_predicate;
        friend smem_db_predicate<int64_t>;
        friend smem_db_predicate<smem_param_container::page_choices>;
        friend smem_db_predicate<smem_param_container::opt_choices>;
        friend smem_db_predicate<boolean>;


    public:

        /* General methods */

        SMem_Manager(agent* myAgent);
        ~SMem_Manager() {};

        bool enabled();
        void go(bool store_only);
        void clean_up_for_agent_deletion();
        bool clear();
        void reinit();
        void reset_stats() { statistics->reset(); };

        /* Basic database methods */
        void attach();
        bool connected();
        void reset(Symbol* state);
        void reset_id_counters() { lti_id_counter = (get_max_lti_id() > settings->initial_variable_id->get_value() ? get_max_lti_id() : settings->initial_variable_id->get_value()-1); };
        bool backup_db(const char* file_name, std::string* err);
        bool export_smem(uint64_t lti_id, std::string& result_text, std::string** err_msg);
        void close();

        /* Methods for smem CLI commands*/
        uint64_t    lti_exists(uint64_t pLTI_ID);
        bool        CLI_add(const char* str_to_LTMs, std::string** err_msg);
        bool        CLI_query(const char* ltms, std::string** err_msg, std::string** result_message, uint64_t number_to_retrieve);
        bool        CLI_remove(const char* ltms, std::string** err_msg, std::string** result_message, bool force = false);
        void        set_id_counter(uint64_t counter_value);


        /* Methods for creating an instance of a LTM using STIs */
        void        clear_instance_mappings();
        Symbol*     get_current_iSTI_for_LTI(uint64_t pLTI_ID, goal_stack_level pLevel, char pChar = 'L');

        /* Methods that brings in a portion or all of smem into an ltm_set data structure */
        void        create_store_set(ltm_set* store_set, uint64_t lti_id, uint64_t depth = 1);
        void        create_full_store_set(ltm_set* store_set);
        void        clear_store_set(ltm_set* store_set);

        /* Methods for printing/visualizing semantic memory */
        void        print_store(std::string* return_val);
        void        print_smem_object(uint64_t pLTI_ID, uint64_t depth, std::string* return_val, bool history = false);

        smem_timer_container*           timers; /* The following remains public because used in run_soar.cpp */

    private:

        agent*                          thisAgent;

        uint64_t                        lti_id_counter;
        uint64_t                        smem_validation;
        int64_t                         smem_max_cycle;

        smem_statement_container*       SQL;
        smem_param_container*           settings;
        smem_stat_container*            statistics;
        soar_module::sqlite_database*   DB;

        /* Temporary maps used when creating an instance of an LTM */
        id_to_sym_map                   lti_to_sti_map;
        sym_to_id_map                   iSti_to_lti_map;

        /* Methods for smem link interface */
        void            clear_result(Symbol* state);
        void            respond_to_cmd(bool store_only);

        /* Utility methods for smem database */
        void            init_db();
        bool            is_version_one_db();
        void            update_schema_one_to_two();
        void            switch_to_memory_db(std::string& buf);
        void            store_globals_in_db();
        void            variable_create(smem_variable_key variable_id, int64_t variable_value);
        void            variable_set(smem_variable_key variable_id, int64_t variable_value);
        bool            variable_get(smem_variable_key variable_id, int64_t* variable_value);
        wme_list*       get_direct_augs_of_id(Symbol* id, tc_number tc = NIL);

        /* Methods for database hashing */
        smem_hash_id    hash_add_type(byte symbol_type);
        smem_hash_id    hash_int(int64_t val, bool add_on_fail = true);
        smem_hash_id    hash_float(double val, bool add_on_fail = true);
        smem_hash_id    hash_str(char* val, bool add_on_fail = true);
        smem_hash_id    hash(Symbol* sym, bool add_on_fail = true);
        int64_t         rhash__int(smem_hash_id hash_value);
        double          rhash__float(smem_hash_id hash_value);
        void            rhash__str(smem_hash_id hash_value, std::string& dest);
        Symbol*         rhash_(byte symbol_type, smem_hash_id hash_value);

        /* Methods for LTIs */
        uint64_t        add_new_LTI();
        uint64_t        add_specific_LTI(uint64_t lti_id);
        void            get_lti_name(uint64_t pLTI_ID, std::string &lti_name) { lti_name.append("@");  lti_name.append(std::to_string(pLTI_ID)); }
        uint64_t        get_max_lti_id();
        double          lti_activate(uint64_t pLTI_ID, bool add_access, uint64_t num_edges = SMEM_ACT_MAX);
        double          lti_calc_base(uint64_t pLTI_ID, int64_t time_now, uint64_t n = 0, uint64_t activations_first = 0);
        id_set          print_LTM(uint64_t pLTI_ID, double lti_act, std::string* return_val, std::list<uint64_t>* history = NIL);

        /* Methods for retrieving an LTM structure to be installed in STM */
        void            add_triple_to_recall_buffer(symbol_triple_list& my_list, Symbol* id, Symbol* attr, Symbol* value);
        void            install_buffered_triple_list(Symbol* state, wme_set& cue_wmes, symbol_triple_list& my_list, bool meta, bool stripLTILinks = false);
        void            install_memory(Symbol* state, uint64_t pLTI_ID, Symbol* lti, bool activate_lti, symbol_triple_list& meta_wmes, symbol_triple_list& retrieval_wmes, smem_install_type install_type = wm_install, uint64_t depth = 1, std::set<uint64_t>* visited = NULL);
        void            install_recall_buffer(Symbol* state, wme_set& cue_wmes, symbol_triple_list& meta_wmes, symbol_triple_list& retrieval_wmes, bool stripLTILinks = false);

        /* Methods to update/store LTM in smem database */
        void            deallocate_ltm(ltm_object* ltm, bool free_ltm = true);
        void            disconnect_ltm(uint64_t pLTI_ID);
        ltm_slot*       make_ltm_slot(ltm_slot_map* slots, Symbol* attr);
        bool            parse_add_clause(soar::Lexer* lexer, str_to_ltm_map* ltms, ltm_set* newbies);
        Symbol*         parse_constant_attr(soar::Lexeme* lexeme);
        void            store_new(Symbol* pSTI, smem_storage_type store_type, bool pOverwriteOldLinkToLTM, tc_number tc = NIL);
        void            update(Symbol* pSTI, smem_storage_type store_type, tc_number tc = NIL);
        void            STM_to_LTM(Symbol* pSTI, smem_storage_type store_type, bool pCreateNewLTM, bool pOverwriteOldLinkToLTM, tc_number tc = NIL);
        void            LTM_to_DB(uint64_t pLTI_ID, ltm_slot_map* children, bool remove_old_children, bool activate);

        /* Methods for creating an instance of a LTM using STIs */
        uint64_t        get_current_LTI_for_iSTI(Symbol* pSTI, bool useLookupTable, bool pOverwriteOldLinkToLTM);

        /* Methods for queries */
        bool                            process_cue_wme(wme* w, bool pos_cue, smem_prioritized_weighted_cue& weighted_pq, MathQuery* mathQuery);
        uint64_t                        process_query(Symbol* state, Symbol* query, Symbol* negquery, Symbol* mathQuery, id_set* prohibit, wme_set& cue_wmes, symbol_triple_list& meta_wmes, symbol_triple_list& retrieval_wmes, smem_query_levels query_level = qry_full, uint64_t number_to_retrieve = 1, std::list<uint64_t>* match_ids = NIL, uint64_t depth = 1, smem_install_type install_type = wm_install);
        std::pair<bool, bool>*          processMathQuery(Symbol* mathQuery, smem_prioritized_weighted_cue* weighted_pq);
        soar_module::sqlite_statement*  setup_web_crawl(smem_weighted_cue_element* el);


};

#endif
