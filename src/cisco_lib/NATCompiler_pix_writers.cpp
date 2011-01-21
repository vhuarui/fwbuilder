/* 

                          Firewall Builder

                 Copyright (C) 2002 NetCitadel, LLC

  Author:  Vadim Kurland     vadim@vk.crocodile.org

  $Id$

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

#include "config.h"

#include "NATCompiler_pix.h"

#include "fwbuilder/FWObjectDatabase.h"
#include "fwbuilder/RuleElement.h"
#include "fwbuilder/NAT.h"
#include "fwbuilder/AddressRange.h"
#include "fwbuilder/ICMPService.h"
#include "fwbuilder/TCPService.h"
#include "fwbuilder/UDPService.h"
#include "fwbuilder/Interface.h"
#include "fwbuilder/IPv4.h"
#include "fwbuilder/Network.h"
#include "fwbuilder/Resources.h"

#include <algorithm>
#include <functional>

#include <assert.h>

using namespace libfwbuilder;
using namespace fwcompiler;
using namespace std;


string NATCompiler_pix::PrintRule::_printAddress(Address *a,bool print_netmask)
{
    string addr = a->getAddressPtr()->toString();
    string mask = a->getNetmaskPtr()->toString();
    if (addr=="0.0.0.0" && mask=="0.0.0.0") return "any";
//    if (addr=="0.0.0.0") addr="0";
//    if (mask=="0.0.0.0") mask="0";

/* 
 * If the object 'a' is a Host or a IPv4 (that is, it defines only
 * a single IP address) but its netmask is not 255.255.255.255, PIX will
 * issue an error "address,mask doesn't pair".
 *
 *  I am not sure if it is appropriate to just fix this for the user,
 *  may be I should issue a warning or even abort.
 */
    if (Host::isA(a) || IPv4::isA(a)) mask="255.255.255.255";

    if (mask=="255.255.255.255") { addr="host "+addr; mask=""; }
    if (print_netmask) return  addr+" "+mask;
    else return addr;
}


NATCompiler_pix::PrintRule::PrintRule(const std::string &name) : NATRuleProcessor(name) 
{ 
    init=true; 
}

/*
 * we verify that port ranges are not used in verifyRuleElements
 */
void NATCompiler_pix::PrintRule::_printPort(Service *srv)
{
    if (TCPService::isA(srv) || UDPService::isA(srv))
    {
	int drs=TCPUDPService::cast(srv)->getDstRangeStart();

	if (drs!=0)   compiler->output << drs << " ";
    }
}

string NATCompiler_pix::PrintRule::_printSrcService(Service *srv)
{
    ostringstream  str;

    if (TCPService::isA(srv) || UDPService::isA(srv)) 
    {
	int rs=TCPUDPService::cast(srv)->getSrcRangeStart();
	int re=TCPUDPService::cast(srv)->getSrcRangeEnd();

        if (rs<0) rs=0;
        if (re<0) re=0;

	if (rs>0 || re>0) {
	    if (rs==re)  str << "eq " << rs;
	    else
		if (rs==0 && re!=0)      str << "lt " << re;
		else
		    if (rs!=0 && re==65535)  str << "gt " << rs;
		    else
			str << "range " << rs << " " << re;
	}
    }
    return str.str();
}

string NATCompiler_pix::PrintRule::_printDstService(Service *srv)
{
    ostringstream  str;

    if (TCPService::isA(srv) || UDPService::isA(srv)) {
	int rs=TCPUDPService::cast(srv)->getDstRangeStart();
	int re=TCPUDPService::cast(srv)->getDstRangeEnd();

        if (rs<0) rs=0;
        if (re<0) re=0;

	if (rs>0 || re>0) {
	    if (rs==re)  str << "eq " << rs;
	    else
		if (rs==0 && re!=0)      str << "lt " << re;
		else
		    if (rs!=0 && re==65535)  str << "gt " << rs;
		    else
			str << "range " << rs << " " << re;
	}
    }
    if (ICMPService::isA(srv) && srv->getInt("type")!=-1)
	    str << srv->getStr("type") << " ";
    return str.str();
}

string NATCompiler_pix::PrintRule::_printConnOptions(NATRule *rule)
{
    if (rule==NULL) return "";

    ostringstream ostr;

    int max_conns=compiler->fw->getOptionsObject()->getInt("pix_max_conns");
    int emb_limit=compiler->fw->getOptionsObject()->getInt("pix_emb_limit");

    if (max_conns<0) max_conns=0;
    if (emb_limit<0) emb_limit=0;

    // we only support tcp connection options at this time
    // however PIX 7.0 (7.2?) also supports udp conn limit
    //
    // Note that keyword 'tcp' here is only valid in 7.x
    if (libfwbuilder::XMLTools::version_compare(
            compiler->fw->getStr("version"),"7.0")>=0) ostr << "tcp ";

    ostr << max_conns << " " << emb_limit;
    return ostr.str();
}



void NATCompiler_pix::PrintRule::printNONAT(NATRule *rule)
{
    Helper helper(compiler);
    NATCompiler_pix *pix_comp = dynamic_cast<NATCompiler_pix*>(compiler);

    string platform = compiler->fw->getStr("platform");
    string version = compiler->fw->getStr("version");
    string clearACLcmd = Resources::platform_res[platform]->getResourceStr(
        string("/FWBuilderResources/Target/options/")+
        "version_"+version+"/pix_commands/clear_acl");

    Address  *osrc=compiler->getFirstOSrc(rule);  assert(osrc);
    Address  *odst=compiler->getFirstODst(rule);  assert(odst);
    Service  *osrv=compiler->getFirstOSrv(rule);  assert(osrv);
                                      
    Address  *tsrc=compiler->getFirstTSrc(rule);  assert(tsrc);
    Address  *tdst=compiler->getFirstTDst(rule);  assert(tdst);
    Service  *tsrv=compiler->getFirstTSrv(rule);  assert(tsrv);


    switch (rule->getInt("nonat_type"))
    {
    case NONAT_NAT0:
    {
        nonat n0 = pix_comp->nonat_rules[rule->getId()];
        Interface *iface1 = n0.i_iface;
//        Interface *iface2=n0.o_iface;

        if (rule->getBool("use_nat_0_0"))
        {
/* old, < 6.3 */
            compiler->output << "nat (" << iface1->getLabel() << ") 0 0 0";
            compiler->output << endl;
        } else
        {
/* new, >=6.3 */

            compiler->output << endl;

            if (pix_comp->getACLFlag(n0.acl_name)==0 && 
                compiler->fw->getOptionsObject()->getBool(
                    "pix_acl_substitution"))
            {
                compiler->output << clearACLcmd <<" " << n0.acl_name << endl;
                pix_comp->setACLFlag(n0.acl_name,1);
            }

            compiler->output << "access-list " 
                             << n0.acl_name 
                             << " permit ip "
                             << _printAddress(n0.src,true)
                             << " "
                             << _printAddress(n0.dst,true)
                             << endl;

            if (pix_comp->first_nonat_rule_id[iface1->getId()]==rule->getId())
            {
                if (compiler->fw->getStr("platform")=="fwsm" && 
                    compiler->fw->getOptionsObject()->getBool(
                        "pix_use_manual_commit") )
                {
                    compiler->output << "access-list commit" << endl;
                    compiler->output << endl;
                }
                compiler->output << "nat (" 
                                 << iface1->getLabel() 
                                 << ") 0 access-list " 
                                 << n0.acl_name 
                                 << endl;
            }
        }
        break;
    }
    case NONAT_STATIC:
    {
	Address  *osrc=compiler->getFirstOSrc(rule);  assert(osrc);
	Address  *odst=compiler->getFirstODst(rule);  assert(odst);

        Interface *osrc_iface = Interface::cast(
            compiler->dbcopy->findInIndex(helper.findInterfaceByNetzone(osrc)));
        Interface *odst_iface = Interface::cast(
            compiler->dbcopy->findInIndex(helper.findInterfaceByNetzone(odst)));

        string addr=odst->getAddressPtr()->toString();
	string mask;
	if (Network::isA(odst)) mask=odst->getNetmaskPtr()->toString();
	else mask="255.255.255.255";

        compiler->output << "static (" 
                         << odst_iface->getLabel() << ","
                         << osrc_iface->getLabel() << ") "
                         << addr << " " << addr 
                         << " netmask " << mask
                         << endl;
    }
        break;
    }
}

void NATCompiler_pix::PrintRule::printSNAT(NATRule *rule)
{
    NATCompiler_pix *pix_comp = dynamic_cast<NATCompiler_pix*>(compiler);
    NATCmd *natcmd = pix_comp->nat_commands[ rule->getInt("nat_cmd") ];
    string platform = compiler->fw->getStr("platform");
    string version = compiler->fw->getStr("version");
    string clearACLcmd = Resources::platform_res[platform]->getResourceStr(
        string("/FWBuilderResources/Target/options/")+
        "version_"+version+"/pix_commands/clear_acl");

    Address  *osrc=compiler->getFirstOSrc(rule);  assert(osrc);
    Address  *odst=compiler->getFirstODst(rule);  assert(odst);
    Service  *osrv=compiler->getFirstOSrv(rule);  assert(osrv);
                                      
    Address  *tsrc=compiler->getFirstTSrc(rule);  assert(tsrc);
    Address  *tdst=compiler->getFirstTDst(rule);  assert(tdst);
    Service  *tsrv=compiler->getFirstTSrv(rule);  assert(tsrv);

    if ( ! natcmd->ignore_global)
    {
        compiler->output << 
            "global (" << natcmd->t_iface->getLabel() << ") " 
                         << natcmd->nat_id;
            
        switch (natcmd->type) 
        {
        case INTERFACE:
            compiler->output << " interface" << endl;
            break;
        case SINGLE_ADDRESS:
            compiler->output << " " 
                             << natcmd->t_addr->getAddressPtr()->toString()
                             << endl;
            break;
        case NETWORK_ADDRESS:
            compiler->output << " " 
                             << natcmd->t_addr->getAddressPtr()->toString() 
                             << " netmask "
                             << natcmd->t_addr->getNetmaskPtr()->toString()
                             << endl;
            break;
        case ADDRESS_RANGE:
        {
            AddressRange *ar=AddressRange::cast(natcmd->t_addr);
            compiler->output << " " 
                             << ar->getRangeStart().toString()
                             << "-"
                             << ar->getRangeEnd().toString()
                             << " netmask "
                             << natcmd->t_iface->getNetmaskPtr()->toString()
                             << endl;
        }
        break;
        default: ;    // TODO: should actually be always_assert
        }
    }

    if ( natcmd->ignore_nat)
    {
        compiler->output <<"! " << natcmd->comment << endl;
    } else
    {
        if (rule->getBool("use_nat_0_0") ||
            libfwbuilder::XMLTools::version_compare(compiler->fw->getStr("version"),"6.3")<0)
        {
/* old, < 6.3 */
            compiler->output
                << "nat (" << natcmd->o_iface->getLabel() << ") "
                << natcmd->nat_id
                << " "
                << natcmd->o_src->getAddressPtr()->toString()  << " "
                << natcmd->o_src->getNetmaskPtr()->toString();
            if (natcmd->outside)
                compiler->output << " outside";
            else
                compiler->output << " " << _printConnOptions(rule);
            compiler->output << endl;
        } else
        {
/* new, >=6.3 */
            if (pix_comp->getACLFlag(natcmd->nat_acl_name)==0 && 
                compiler->fw->getOptionsObject()->getBool("pix_acl_substitution"))
            {
                compiler->output << clearACLcmd << " "
                                 << natcmd->nat_acl_name
                                 << endl;
                pix_comp->setACLFlag(natcmd->nat_acl_name,1);
            }

            compiler->output << "access-list " 
                             << natcmd->nat_acl_name 
                             << " permit ";
            compiler->output << osrv->getProtocolName();
            compiler->output << " ";
            compiler->output << _printAddress(osrc,true);
            compiler->output << " ";
            compiler->output << _printSrcService( osrv );
            compiler->output << " ";
            compiler->output << _printAddress(odst,true);
            compiler->output << " ";
            compiler->output << _printDstService( osrv );
            compiler->output << endl;

            if (!natcmd->ignore_nat_and_print_acl)
            {
                if (compiler->fw->getStr("platform")=="fwsm" && 
                    compiler->fw->getOptionsObject()->getBool("pix_use_manual_commit") )
                {
                    compiler->output << "access-list commit" << endl;
                    compiler->output << endl;
                }
                compiler->output << "nat (" << natcmd->o_iface->getLabel() << ") "
                                 << natcmd->nat_id
                                 << " access-list "
                                 << natcmd->nat_acl_name;
                if (natcmd->outside)  compiler->output << " outside";
                else                  compiler->output << " " << _printConnOptions(rule);
                compiler->output << endl;
            }
        }

    }
}

void NATCompiler_pix::PrintRule::printSDNAT(NATRule*)
{
}

void NATCompiler_pix::PrintRule::printDNAT(NATRule *rule)
{
    NATCompiler_pix *pix_comp = dynamic_cast<NATCompiler_pix*>(compiler);
    string platform = compiler->fw->getStr("platform");
    string version = compiler->fw->getStr("version");
    string clearACLcmd = Resources::platform_res[platform]->getResourceStr(
        string("/FWBuilderResources/Target/options/")+
        "version_"+version+"/pix_commands/clear_acl");

    Address  *osrc=compiler->getFirstOSrc(rule);  assert(osrc);
    Address  *odst=compiler->getFirstODst(rule);  assert(odst);
    Service  *osrv=compiler->getFirstOSrv(rule);  assert(osrv);
                                      
    Address  *tsrc=compiler->getFirstTSrc(rule);  assert(tsrc);
    Address  *tdst=compiler->getFirstTDst(rule);  assert(tdst);
    Service  *tsrv=compiler->getFirstTSrv(rule);  assert(tsrv);

    Interface *iface_orig = Interface::cast(
        compiler->dbcopy->findInIndex(rule->getInt("nat_iface_orig")));
    Interface *iface_trn  = Interface::cast(
        compiler->dbcopy->findInIndex(rule->getInt("nat_iface_trn")));

    StaticCmd *scmd = pix_comp->static_commands[ rule->getInt("sc_cmd") ];

    const InetAddr *outa = scmd->oaddr->getAddressPtr();
    const InetAddr *outm = scmd->oaddr->getNetmaskPtr();
    const InetAddr *insa = scmd->iaddr->getAddressPtr();
/*
 * we verify that odst and tdst have the same size in verifyRuleElements,
 * so we can rely on that now.
 */

    if (libfwbuilder::XMLTools::version_compare(compiler->fw->getStr("version"),"6.3")<0)
    {
/* old, < 6.3 */

        compiler->output << "static ("
                         << iface_trn->getLabel()
                         << ","
                         << iface_orig->getLabel()
                         << ") " ;

        bool use_ports=false;

        if (TCPService::cast(osrv)) { use_ports=true; compiler->output << "tcp "; }
        if (UDPService::cast(osrv)) { use_ports=true; compiler->output << "udp "; }

        if (Interface::cast(scmd->oaddr)!=NULL) 
        {
            compiler->output << "interface ";
            if (use_ports)	_printPort(scmd->osrv);

            compiler->output << insa->toString() << " ";
            if (use_ports)	_printPort(scmd->tsrv);
        } else
        {
            compiler->output << outa->toString() << " ";
            if (use_ports)	_printPort(scmd->osrv);

            compiler->output << insa->toString() << " ";
            if (use_ports)	_printPort(scmd->tsrv);

            compiler->output << " netmask " << outm->toString();
        }
        compiler->output << " " << _printConnOptions(rule) << endl;
    } else
    {
/* new, >=6.3 */

        if (pix_comp->getACLFlag(scmd->acl_name)==0 && 
            compiler->fw->getOptionsObject()->getBool("pix_acl_substitution"))
        {
            compiler->output << clearACLcmd << " "
                             << scmd->acl_name
                             << endl;
            pix_comp->setACLFlag(scmd->acl_name,1);
        }

        compiler->output << "access-list " 
                         << scmd->acl_name
                         << " permit ";
/*
 *  This acl does not make any sense if treated as a regular access
 *  list. I just follow example from
 *  http://www.cisco.com/en/US/products/sw/secursw/ps2120/products_configuration_guide_chapter09186a0080172786.html#1113601
 */
        compiler->output << scmd->osrv->getProtocolName();
        compiler->output << " ";
        compiler->output << _printAddress(scmd->iaddr,true);
        compiler->output << " ";
        compiler->output << _printDstService( scmd->tsrv );
        compiler->output << " ";
        compiler->output << _printAddress(scmd->osrc,true);
        compiler->output << " ";
        compiler->output << _printSrcService( scmd->osrv );
        compiler->output << endl;

        if (!scmd->ignore_scmd_and_print_acl)
        {
            if (compiler->fw->getStr("platform")=="fwsm" && 
                compiler->fw->getOptionsObject()->getBool("pix_use_manual_commit") )
            {
                compiler->output << "access-list commit" << endl;
                compiler->output << endl;
            }
            compiler->output << "static ("
                             << iface_trn->getLabel()
                             << ","
                             << iface_orig->getLabel()
                             << ") " ;

            bool use_ports=false;
            if (TCPService::cast(scmd->osrv)) { use_ports=true; compiler->output << "tcp "; }
            if (UDPService::cast(scmd->osrv)) { use_ports=true; compiler->output << "udp "; }

            if (Interface::cast(scmd->oaddr)!=NULL)
                compiler->output << "interface ";
            else
                compiler->output << outa->toString() << " ";
            if (use_ports) _printPort(scmd->osrv);
            compiler->output << " ";

            compiler->output << "access-list "
                             << scmd->acl_name
                             << " " << _printConnOptions(rule) << endl;
        }
    }

}


bool NATCompiler_pix::PrintRule::processNext()
{
    string platform = compiler->fw->getStr("platform");
    string version = compiler->fw->getStr("version");
    string clearACLcmd = Resources::platform_res[platform]->getResourceStr(
        string("/FWBuilderResources/Target/options/")+
        "version_"+version+"/pix_commands/clear_acl");

    NATRule *rule = getNext(); if (rule==NULL) return false;
    tmp_queue.push_back(rule);

    bool suppress_comments =
        ! compiler->fw->getOptionsObject()->getBool("pix_include_comments");

    compiler->output << compiler->printComment(
        rule, current_rule_label, "!", suppress_comments);

    switch (rule->getRuleType()) 
    {
    case NATRule::NONAT:
        printNONAT(rule);
        break;

    case NATRule::SNAT:
    {
        printSNAT(rule);
        break;
    }

    case NATRule::SDNAT:
    {
        printSDNAT(rule);
        break;
    }

    case NATRule::DNAT:
    {
        printDNAT(rule);
        break;
    }
    default: ;    // TODO: should actually be always_assert

    }
    return true;
}


