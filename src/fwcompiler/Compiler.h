/* 

                          Firewall Builder

                 Copyright (C) 2002 NetCitadel, LLC

  Author:  Vadim Kurland     vadim@vk.crocodile.org

  $Id: Compiler.h 966 2006-08-18 03:59:32Z vkurland $

  This program is free software which we release under the GNU General Public
  License. You may redistribute and/or modify this program under the terms
  of that license as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  To get a copy of the GNU General Public License, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef __COMPILER_HH__
#define __COMPILER_HH__

#include <fwbuilder/libfwbuilder-config.h>
#include "fwbuilder/FWException.h"

#include "fwcompiler/RuleProcessor.h"

#include <list>
#include <vector>
#include <map>

#include <fstream>
#include <sstream>

namespace libfwbuilder {
    class FWObject;
    class FWOptions;
    class FWObjectDatabase;
    class InetAddr;
    class Address;
    class Service;
    class Interval;
    class Network;
    class Firewall;
    class Interface;
    class Rule;
    class RuleSet;
    class PolicyRule;
    class NATRule;
    class RuleElement;
};


namespace fwcompiler {

    class OSConfigurator;

    class FWCompilerException : public libfwbuilder::FWException {
	libfwbuilder::Rule *rule;
	public:
	FWCompilerException(libfwbuilder::Rule *r,const std::string &err);
	libfwbuilder::Rule *getRule() const { return rule; }
    };

/* 
 * operations    (see Compiler_ops.cc)
 */

    /**
     * this method compares two objects to determine if one of them
     * "shades" another
     */
    bool checkForShadowing(const libfwbuilder::Address &o1,const libfwbuilder::Address &o2);
    bool checkForShadowing(const libfwbuilder::Service &o1,const libfwbuilder::Service &o2);

    /**
     * this operator compares two objects to determine if they are
     * equivalent
     */
    bool operator==(const libfwbuilder::Address &o1,const libfwbuilder::Address &o2);
    bool operator==(const libfwbuilder::Service &o1,const libfwbuilder::Service &o2);
    bool operator==(const libfwbuilder::Interval &o1,const libfwbuilder::Interval &o2);

    /**
     * this method finds intersection of two objects. Objects must
     * be of such types that have address (Host, Firewall,
     * Interface, Network) , otherwise it throws an exception
     *
     * TODO:  implement this as a virtual method of respective classes
     *
     * this method is intended for internal use only
     */
    std::vector<libfwbuilder::FWObject*> 
    _find_obj_intersection(libfwbuilder::Address *o1,
			   libfwbuilder::Address *o2);
    /**
     * this method finds intersection of two services. If one or
     * both objects are not services, it throws exception
     *
     * this method is intended for internal use only
     */
    std::vector<libfwbuilder::FWObject*> 
    _find_srv_intersection(libfwbuilder::Service *o1,
			   libfwbuilder::Service *o2);

    /**
     * this method finds intersection of two ranges of ports
     *
     * this method is intended for internal use only
     */
    bool _find_portrange_intersection(int rs1,int re1,int rs2,int re2,int &rsr,int &rer);
	



    class Compiler {

        void _init(libfwbuilder::FWObjectDatabase *_db,
                   const std::string &fwname);

        virtual void _expand_group_recursive(libfwbuilder::FWObject *o,
				     std::list<libfwbuilder::FWObject*> &ol);

        virtual void _expand_addr_recursive(libfwbuilder::Rule *rule,
                                    libfwbuilder::FWObject *s,
                                    std::list<libfwbuilder::FWObject*> &ol);

        bool _complexMatchWithInterface(libfwbuilder::Address   *obj1,
                                        libfwbuilder::Interface *iface,
                                        bool recognize_broadcasts=true);

        bool _complexMatchWithAddress(const libfwbuilder::InetAddr *obj1_addr,
                                      libfwbuilder::Interface *iface,
                                      const std::string &address_type,
                                      bool recognize_broadcasts);
	protected:

        int  _cntr_;
        bool initialized;
        int countIPv6Rules;
        bool ipv6;

        std::list<BasicRuleProcessor*> rule_processors;

        /**
         * if object <o> is Address, check if it matches address family
         * (i.e. if it is IPv6 or IPv4). If it is service, always return true.
         */
        bool MatchesAddressFamily(libfwbuilder::FWObject *o);
        
        /**
         * this method finds intersection of two atomic rules. Resulting
         * rule may have multiple objects in src,dst and srv, so
         * converting to atomic may be necessary. If rules can not be
         * compared, then it throws an exception. If rules are compatible
         * but have nothing in common, then this method returns rule with
         * empty src,dst,srv. Use isEmpty(Rule &r) to check for this
         * condition
         * 
         * This method creates and returns new object of class Rule
         * and does not modify r1 and r2
         *
         * This method works only with interface, src, dst and srv and
         * completely ignores action and other rule options.
         *
         */
        void  getIntersection(const libfwbuilder::PolicyRule &r1,
                              const libfwbuilder::PolicyRule &r2,
                              libfwbuilder::PolicyRule &res);

        /**
         * this function checks if two rules intersect - that is, if there
         * is a non-empty intersection for each rule element. This
         * function does not calculate intersection, it just verifies that
         * it does exsit.
         */
        bool  intersect(const libfwbuilder::PolicyRule &r1,
                        const libfwbuilder::PolicyRule &r2);


        /**
         * add rule processor to the list
         */
        void add(BasicRuleProcessor* rp);

        /**
         *  assembles chain of rule processors and executes it
         */
        void runRuleProcessors();

        /**
         *  deletes chain of rule processors
         */
        void deleteRuleProcessors();

       /*
        * the following variables are simply a cache for frequently used
        * objects
        */
        std::map<const std::string,libfwbuilder::Interface*> fw_interfaces;
        std::string                                          fw_id;
        libfwbuilder::FWOptions                             *fwopt;
        std::map<const std::string,libfwbuilder::FWObject*>  objcache;

        /**
         * stores object o and all its children in the cache, recursively
         */
        int cache_objects(libfwbuilder::FWObject *o);


	public:

        void registerIPv6Rule() { countIPv6Rules++; }
        bool haveIPv6Rules() { return countIPv6Rules > 0; }

        /**
         * returns first object referenced by given rule
         * element. Dereferences FWReference if first object is
         * reference. Uses cache, therefore is faster than
         * RuleElement::getFirst(true)
         */
        libfwbuilder::Address*   getFirstSrc(const libfwbuilder::PolicyRule *rule);
        libfwbuilder::Address*   getFirstDst(const libfwbuilder::PolicyRule *rule);
        libfwbuilder::Service*   getFirstSrv(const libfwbuilder::PolicyRule *rule);
        libfwbuilder::Interval*  getFirstWhen(const libfwbuilder::PolicyRule *rule);
        libfwbuilder::Interface* getFirstItf(const libfwbuilder::PolicyRule *rule);
        
        libfwbuilder::Address* getFirstOSrc(const libfwbuilder::NATRule *rule);
        libfwbuilder::Address* getFirstODst(const libfwbuilder::NATRule *rule);
        libfwbuilder::Service* getFirstOSrv(const libfwbuilder::NATRule *rule);
                                                         
        libfwbuilder::Address* getFirstTSrc(const libfwbuilder::NATRule *rule);
        libfwbuilder::Address* getFirstTDst(const libfwbuilder::NATRule *rule);
        libfwbuilder::Service* getFirstTSrv(const libfwbuilder::NATRule *rule);


	/**
	 *   a method to check for unnumbered interface in a rule
	 *   element (one can not use unnumbered interfaces in rules).
	 */
        bool catchUnnumberedIfaceInRE(libfwbuilder::RuleElement *re);

        /**
         * return true if any address object in source or destination is
         * of given type (can be IPv4 or IPv6).
         */
        bool FindAddressFamilyInRE(libfwbuilder::FWObject *re, bool ipv6);

        /**
         * find ipv6 or ipv4 address objects in the given rule element
         * and remove reference to them
         */
        void DropAddressFamilyInRE(libfwbuilder::RuleElement *rel,
                                   bool drop_ipv6);
        /**
         *  rule processor that "injects" rules into the conveyor
         */
        class Begin : public BasicRuleProcessor
        {
            bool init;
            public:
            Begin();
            Begin(const std::string &n);
            virtual bool processNext();
        };

	/**
	 * this processor prints number of rules in the queue on cout
	 * if compiler->verbose is true
	 */
        class printTotalNumberOfRules : public BasicRuleProcessor
        {
            public:
            virtual bool processNext();
        };

	/**
	 * this processor creates what amounts to the new compiler
	 * pass: it slurps all rules into buffer, then prints its own
	 * name on cout and the releases rules to the next processor
	 * one at a time.  This way processors after this one in the
	 * chain get to start working on the whole rule set from its
	 * beginning.
	 */
        class createNewCompilerPass : public BasicRuleProcessor
        {
            std::string pass_name;
            public:
            createNewCompilerPass(const std::string &_name) : BasicRuleProcessor("New compiler pass") { pass_name=_name; };
            virtual bool processNext();
        };

	/**
	 * this processor prints rule numbers on cout (trivial
	 * progress indicator)
	 */
        class simplePrintProgress : public BasicRuleProcessor
        {
            std::string current_rule_label;
            public:
            simplePrintProgress() : BasicRuleProcessor("Print progress") {};
            virtual bool processNext();
        };

        /**
         * this processor splits rule if one of its rule elements
         * contains firewall itself. This processor is actually only a
         * base class. Derive it and pass rule element type name as a
         * second argument of its constructor.
         */
        class splitIfRuleElementMatchesFW : public PolicyRuleProcessor
        {
            std::string re_type;
            public:
            splitIfRuleElementMatchesFW(const std::string &n,
                                                  std::string _type) :
                PolicyRuleProcessor(n) { re_type=_type; }
            virtual bool processNext();
        };

        /**
         *  eliminates duplicates in RuleElement 're_type'. Inherit
         *  your own class using this one and supply actual rule
         *  element type name through its constructor. 
         *
         *  Function class equalObj compares IDs of two object and
         *  declares objects equal if their ID are the same.  To
         *  change comparison algorithm, inherit from this class,
         *  overload its operator(), then create its instance in
         *  constructor of eliminateDuplicatesInRE and assign to
         *  member 'comparator'
         */
        class equalObj {
            protected:
            libfwbuilder::FWObject *obj;
            public:
            equalObj(){obj=NULL;}
            virtual ~equalObj() {}
            void set(libfwbuilder::FWObject *o) {obj=o;}
            virtual bool operator()(libfwbuilder::FWObject *o);
        };

        class eliminateDuplicatesInRE : public BasicRuleProcessor
        {
            std::string re_type;
            protected:
            equalObj *comparator;
            public:
            eliminateDuplicatesInRE(const std::string &n,const std::string _type):
                BasicRuleProcessor(n) { re_type=_type; comparator=NULL; }
            ~eliminateDuplicatesInRE() { if (comparator!=NULL) delete comparator; }
            virtual bool processNext();
        };

        /**
         * this processor checks for recursive groups, i.e. groups
         * that reference themselves
         */
        class recursiveGroupsInRE : public BasicRuleProcessor
        {
            std::string re_type;
            void isRecursiveGroup(const std::string &grid,libfwbuilder::FWObject *gr);
            public:
            recursiveGroupsInRE(const std::string &n,const std::string &_type) : 
                BasicRuleProcessor(n) { re_type=_type; }
            virtual bool processNext();
        };


	/**
	 * This rule processor checks for empty groups. Normally this
	 * is a fatal error and compilation should be aborted, but
	 * firewall option "ignore_rules_with_empty_groups" causes
	 * compiler to remove this object from the rule element and
	 * drop the rule all together if there are no more objects
	 * left (rule element becomes "any") and continue work
	 * (warning should be issued though).
	 */
        class emptyGroupsInRE : public BasicRuleProcessor
        {
            std::string re_type;
            int  countChildren(libfwbuilder::FWObject *obj);
            void findEmptyGroupsInRE();
            public:
            emptyGroupsInRE(const std::string &n,const std::string &_type) : 
                BasicRuleProcessor(n) { re_type=_type; }
            virtual bool processNext();
        };


        /**
         * Replace MultiAddress objects that require run-time address
         * expansion with corresponding MultiAddressRunTime objects
         */
        class swapMultiAddressObjectsInRE : public BasicRuleProcessor
        {
            std::string re_type;
            public:
            swapMultiAddressObjectsInRE(const std::string &name,
                      const std::string &t) : BasicRuleProcessor(name) { re_type=t; }
            virtual bool processNext();
        };

	/**
	 * generic rule debugger: prints name of the previous rule
	 * processor in a chain and then a rule if its number is
	 * compiler->debug_rule.  Uses virtual method
	 * Compiler::debugPrintRule to actually print the rule
	 */
        class Debug : public BasicRuleProcessor
        {
            public:
            virtual bool processNext();
        };

	/**
	 * prepare interface string
	 */
        class convertInterfaceIdToStr : public BasicRuleProcessor
        {
            public:
            convertInterfaceIdToStr(const std::string &n) : BasicRuleProcessor(n) {}
            virtual bool processNext();
        };
        
        /**
         * prints rule in some universal format (close to that visible
         * to user in the GUI). Used for debugging purposes
         */
        virtual std::string debugPrintRule(libfwbuilder::Rule *rule);

        /**
         * returns pointer to cached interface
         */
        libfwbuilder::Interface* getCachedFwInterface(const std::string &id)
        { return fw_interfaces[id]; }

        /**
         * returns cached firewall object ID
         */
        std::string getFwId() { return fw_id; }

        /**
         * returns pointer to the cached firewall options object
         */
        libfwbuilder::FWOptions* getCachedFwOpt() { return fwopt; }
        
        /**
         *  stores object with given ID in the cache
         */
        void cacheObj(libfwbuilder::FWObject *o);

        /**
         * does cache lookup for object with given ID
         */
        libfwbuilder::FWObject* getCachedObj(const std::string &id)
        {
            return objcache[id];
        }
	
	/**
	 * internal: scans children of 's' and, if finds host or
	 * firewall with multiple interfaces, replaces reference to
	 * that host or firewall with a set of references to its
	 * interfaces. Argument 's' should be a pointer at either src
	 * or dst in the rule. Some platforms may require alterations
	 * to * this algorithm, that's why it is virtual.
	 */
	virtual void _expandAddr(libfwbuilder::Rule *rule,libfwbuilder::FWObject *s);

        /**
         * internal: scans child objects of interface iface, both IPv4
         * and physAddress, and puts them in the list ol. Policy
         * compilers for platforms that support matching on MAC
         * address should reimplement this method and do whatever is
         * right for them (e.g. create combined address objects to
         * fuse information from IPv4 and physAddress together).
         */
        virtual void _expandInterface(libfwbuilder::Interface *iface,
                                     std::list<libfwbuilder::FWObject*> &ol);

	/**
	 * internal: like _expandAddr, but expands address range
	 * objects
	 */
	void _expandAddressRanges(libfwbuilder::Rule *rule,libfwbuilder::FWObject *s);

	/*
	 * normalizes port range (makes sure that niether range start
	 * nor end is <0 and so on
	 */
	void normalizePortRange(int &rs,int &re);


        /**
         * This method returns true if one of the following conditions is met:
         *
         * 1. obj1 is the same as obj2 (compares ID of both objects), or 
         * 2. obj1 is a child of obj2 on any depth, or
         * 3. address of obj1 matches that of any obj2's interfaces, or 
         * 4. address of obj1 is a broadcast address of one of 
         *    the interfaces of obj2
         * 5. address of obj1 is a broadcast (255.255.255.255)
         */
        bool complexMatch(libfwbuilder::Address *obj1,
                          libfwbuilder::Address *obj2,
                          bool recognize_broadcasts=true,
                          bool recognize_multicasts=true);

        /**
         * This method finds interface of obj2 (which is usually
         * firewall object, but not necessarily so) which is connected
         * to the subnet on which obj1 is located. It also works if
         * obj1 is a network object, in this case it looks for the
         * interface that belongs to that network.
         */
        libfwbuilder::Interface* findInterfaceFor(const libfwbuilder::Address *obj1,
                                                  const libfwbuilder::Address *obj2);
        
        /**
         * This method finds an interface of the firewall obj2 which
         * belongs to the subnet on which obj1 is located and returns
         * IPv4 address object of this interface. It also works if
         * obj1 is a network object, in this case it looks for the
         * interface that belongs to that network.
         * 
         * If obj1 is an Interface object, then corresponding Interface
         * object belonging to the firewall is returned (if found).
         */
        libfwbuilder::FWObject* findAddressFor(const libfwbuilder::Address *o1,
                                               const libfwbuilder::Address *o2);
        

        /**
         * prints error message and aborts the program. If compiler is
         * in testing mode (flag test_mode==true), then just prints
         * the error message and returns.
         */
	void abort(const std::string &errstr) throw(libfwbuilder::FWException);

        /**
         * prints an error message and returns
         */
	void error(const std::string &warnstr);

        /**
         * prints warning message
         */
	void warning(const std::string &warnstr);

	int                                debug;
	int                                debug_rule;
	bool                               verbose;

	fwcompiler::OSConfigurator        *osconfigurator;
	libfwbuilder::FWObjectDatabase    *dbcopy;
	libfwbuilder::Firewall            *fw;

        std::string                        ruleSetName;;
        
	libfwbuilder::RuleSet             *source_ruleset;
	libfwbuilder::RuleSet             *combined_ruleset;
	libfwbuilder::RuleSet             *temp_ruleset;

        libfwbuilder::Group               *temp;

	std::stringstream                  output;

        bool                               test_mode;

	virtual ~Compiler();

	Compiler(libfwbuilder::FWObjectDatabase *_db,
		 const std::string &fwname, bool ipv6_policy);

	Compiler(libfwbuilder::FWObjectDatabase *_db,
		 const std::string &fwname, bool ipv6_policy,
		 fwcompiler::OSConfigurator *_oscnf);

	Compiler(libfwbuilder::FWObjectDatabase *_db, bool ipv6_policy);
        
	void setDebugLevel(int dl) { debug=dl;       }
	void setDebugRule(int dr)  { debug_rule=dr;  }
	void setVerbose(bool v)    { verbose=v;      }
        void setTestMode()         { test_mode=true; }
        void setSourceRuleSet(libfwbuilder::RuleSet *rs) { source_ruleset = rs; }
        libfwbuilder::RuleSet* getSourceRuleSet() { return source_ruleset; }

        void setRuleSetName(const std::string &name) { ruleSetName = name; }
        std::string getRuleSetName() { return ruleSetName; }
        
	std::string getCompiledScript();
        int         getCompiledScriptLength();

	void expandGroupsInRuleElement(libfwbuilder::RuleElement *s);

        /**
	 * this method should return platform name. It is used 
	 * to construct proper error and warning messages.
	 */
	virtual std::string myPlatformName() =0;

	std::string getUniqueRuleLabel();

	virtual std::string createRuleLabel(const std::string &prefix,
                                            const std::string &txt,
                                            int rule_num);

	/**
	 * prolog should pack rules into combined_ruleset and return
	 * number of rules found
	 */
	virtual int  prolog();
	virtual void compile();
	virtual void epilog();

	/**
	 * prints rule marked for debugging (its number * is in
	 * debug_rule member variable)
	 */
        void debugRule();

        /**
         * determine if given rule set is "root" rule set, as opposed
         * to branch.
         */ 
        static bool isRootRuleSet(libfwbuilder::RuleSet*);

        
    };


}

#endif
