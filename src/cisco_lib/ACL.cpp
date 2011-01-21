/* 

                          Firewall Builder

                 Copyright (C) 2004 NetCitadel, LLC

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

#include "ACL.h"

#include <sstream>


using namespace fwcompiler;
using namespace std;


string ciscoACL::addLine(const std::string &s)
{
    acl.push_back(s);
    nlines++;
    return printLastLine();
}

string ciscoACL::quoteLine(const string &s)
{
    if (quote_remarks && s.find(' ') != string::npos)
        return "\"" + s + "\"";
    else
        return s;
}

/*
 * Adds remark to access list. Checks and adds each remark only
 * once. We use rule labels for remarks
 */
string ciscoACL::addRemark(const std::string &rl, const std::string &comment)
{
    string output;
    if (_last_rule_label != rl)
    {
        acl.push_back(" remark " + quoteLine(rl));

        output += printLastLine();
        nlines++;

        if (!comment.empty())
        {
            string::size_type n, c1;
            c1 = 0;
            while ( (n = comment.find("\n", c1)) != string::npos )
            {
                acl.push_back(" remark " + quoteLine(comment.substr(c1, n-c1)));
                output += printLastLine();
                nlines++;
                c1 = n + 1;
            }
            acl.push_back(" remark " + quoteLine(comment.substr(c1)));
            output += printLastLine();
            nlines++;
        }

        _last_rule_label = rl;
        return output;
    }
    return "";
}


string ciscoACL::print()
{
    ostringstream  str;

    for (list<string>::iterator s=acl.begin(); s!=acl.end(); s++)
        str << printLine(*s);

    return str.str();
}

string ciscoACL::printLastLine()
{
    return printLine(acl.back());
}
 
string ciscoACL::printLine(const string &s)
{
    ostringstream  str;

    // _ip_acl means Cisco IOS "ip access-list extended <name>" style ACL
    // actual lines of the access list just start with "permit" or "deny"
    if ( s.find('!')!=0 )
    {
        if (_ip_acl) str << "  ";
        else  str << "access-list " << _workName << " ";
    }
    str << s << endl;

    return str.str();
}

