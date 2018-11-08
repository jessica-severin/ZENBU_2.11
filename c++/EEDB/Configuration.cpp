/*  $Id: Configuration.cpp,v 1.52 2016/04/19 06:35:17 severin Exp $ */

/***
NAME - EEDB::Configuration

SYNOPSIS

DESCRIPTION

CONTACT

Jessica Severin <severin@gsc.riken.jp>

LICENSE

 * Software License Agreement (BSD License)
 * ZENBU [EEDB] system
 * copyright (c) 2007-2013 Jessica Severin RIKEN OSC
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Jessica Severin RIKEN OSC nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

APPENDIX

The rest of the documentation details each of the object methods. Internal methods are usually preceded with a _

***/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sqlite3.h>
#include <stdarg.h>
#include <rapidxml.hpp>  //rapidxml must be include before boost
#include <rapidxml_print.hpp>  //rapidxml must be include before boost
#include <boost/algorithm/string.hpp>
#include <MQDB/MappedQuery.h>
#include <EEDB/Configuration.h>
#include <EEDB/User.h>
#include <EEDB/MetadataSet.h>

using namespace std;
using namespace MQDB;

const char*  EEDB::Configuration::class_name = "Configuration";

EEDB::Configuration::Configuration() {
  init();
}

EEDB::Configuration::~Configuration() {
}

void _eedb_configuration_delete_func(MQDB::DBObject *obj) { 
  delete (EEDB::Configuration*)obj;
}
void _eedb_configuration_xml_func(MQDB::DBObject *obj, string &xml_buffer) { 
  ((EEDB::Configuration*)obj)->_xml(xml_buffer);
}
void _eedb_configuration_simple_xml_func(MQDB::DBObject *obj, string &xml_buffer) { 
  ((EEDB::Configuration*)obj)->_simple_xml(xml_buffer);
}


void EEDB::Configuration::init() {
  MQDB::MappedQuery::init();
  _classname                 = EEDB::Configuration::class_name;
  _funcptr_delete            = _eedb_configuration_delete_func;
  _funcptr_xml               = _eedb_configuration_xml_func;
  _funcptr_simple_xml        = _eedb_configuration_simple_xml_func;

  _metadataset      = NULL;
  _owner            = NULL;
  _collaboration    = NULL;
  _owner_user_id    = -1;
  _collaboration_id = -1;
  _last_access      = 0;
  _create_date      = 0;
  _access_count     = 0;
  _mdata_loaded     = false;
  _symbols_loaded   = false;
}


////////////////////////////////////////////////////
//
// XML and description section
//
////////////////////////////////////////////////////

void EEDB::Configuration::_xml_start(string &xml_buffer) {
  xml_buffer += "<configuration ";
  xml_buffer += "uuid=\"" + _uuid + "\" ";
  xml_buffer += "type=\"" + _config_type + "\" ";
  xml_buffer += "create_date=\"" + create_date_string() + "\" ";  
  xml_buffer += "last_access=\"" + last_access_date_string() + "\" ";  
  
  char buffer[1024];
  snprintf(buffer, 1000, "access_count=\"%ld\"", _access_count);
  xml_buffer += buffer;
  
  xml_buffer += ">";

  if(owner()) { owner()->simple_xml(xml_buffer); }

  if(collaboration()) { collaboration()->simple_xml(xml_buffer); }
  else {
    xml_buffer.append("<collaboration name=\"private\" uuid=\"private\" />");
  }
}


void EEDB::Configuration::_xml_end(string &xml_buffer) {
  xml_buffer.append("</configuration>\n");
}


void EEDB::Configuration::_simple_xml(string &xml_buffer) {
  _xml_start(xml_buffer);

  xml_buffer.append("<mdata type=\"eedb:display_name\">");
  xml_buffer.append(html_escape(display_name()));
  xml_buffer.append("</mdata>");
  
  EEDB::Metadata *md;
  if((md = metadataset()->find_metadata("description", ""))) { md->xml(xml_buffer); }
  if((md = metadataset()->find_metadata("date", ""))) { md->xml(xml_buffer); }
  if((md = metadataset()->find_metadata("eedb:assembly_name", ""))) { md->xml(xml_buffer); }
  if((md = metadataset()->find_metadata("assembly_name", ""))) { md->xml(xml_buffer); }
  
  _xml_end(xml_buffer);
}


void EEDB::Configuration::_xml(string &xml_buffer) {
  _xml_start(xml_buffer);

  xml_buffer.append("<mdata type=\"eedb:display_name\">");
  xml_buffer.append(html_escape(display_name()));
  xml_buffer.append("</mdata>");

  EEDB::MetadataSet *mdset = metadataset();
  if(mdset!=NULL) { mdset->xml_nokeywords(xml_buffer); }

  _xml_end(xml_buffer);
}


EEDB::Configuration::Configuration(void *xml_node) {
  //constructor using a rapidxml node
  init();
  if(xml_node==NULL) { return; }
  
  rapidxml::xml_node<>      *root_node = (rapidxml::xml_node<>*)xml_node;
  rapidxml::xml_node<>      *node, *node2;
  rapidxml::xml_attribute<> *attr;
  
  if(string(root_node->name()) != "configuration") { return; }
  
  if((attr = root_node->first_attribute("uuid")))     { _uuid = attr->value(); }
  if((attr = root_node->first_attribute("type")))      { _config_type = attr->value(); }

  if((attr = root_node->first_attribute("access_count"))) {
    _access_count = strtol(attr->value(), NULL, 10); 
  }

  if((attr = root_node->first_attribute("last_access"))) {
    string date_string = attr->value();
    _last_access = MQDB::seconds_from_epoch(date_string);
  }
  if((attr = root_node->first_attribute("create_date"))) {
    string date_string = attr->value();
    _create_date = MQDB::seconds_from_epoch(date_string);
  }
  
  // user
  if((node = root_node->first_node("eedb_user")) != NULL) {
    _owner         = new EEDB::User(node);
    _owner_user_id = -1;
  }
  
  // collaboration
  if((node = root_node->first_node("collaboration")) != NULL) {
    _collaboration = new EEDB::Collaboration(node);
    _collaboration_id = -1;
    if(_collaboration && (_collaboration->group_uuid() == "private")) {
      //private collaboaration means not defined
      _collaboration->release();
      _collaboration = NULL;
    }    
  }
  
  //
  // metadata
  //
  metadataset();
  if((node = root_node->first_node("mdata")) != NULL) {
    while(node) {
      EEDB::Metadata *mdata = new EEDB::Metadata(node);
      if(mdata->type() == "configXML") {
        node2 = node->first_node(); 
        // Print to string using output iterator
        if(node2) {
          string buffer;
          rapidxml::print(back_inserter(buffer), *node2, 0);
          mdata->release();
          mdata = new EEDB::Metadata("configXML", buffer);
          _metadataset->add_metadata(mdata);
        } else {
          fprintf(stderr, "ERROR parsing configXML [%s]\n", mdata->data().c_str());
        }
      } else {
        _metadataset->add_metadata(mdata);
      }
      node = node->next_sibling("mdata");
    }    
  }
  if((node = root_node->first_node("symbol")) != NULL) {
    while(node) {
      EEDB::Symbol *mdata = new EEDB::Symbol(node);
      _metadataset->add_metadata(mdata);
      node = node->next_sibling("symbol");
    }    
  }
  _mdata_loaded     = true;
  _symbols_loaded   = true;
}


////////////////////////////////////////////////////
//
// getter/setter methods of data which is stored in database
//
////////////////////////////////////////////////////

string  EEDB::Configuration::uuid() { return _uuid; }
void    EEDB::Configuration::uuid(string value) { _uuid = value; }

string  EEDB::Configuration::config_type() { return _config_type; }

void    EEDB::Configuration::config_type(string value) { 
  if(value == "eeDB_gLyphs_configs")      { _config_type = "VIEW"; }
  if(value == "eeDB_gLyph_track_configs") { _config_type = "TRACK"; }
  if(value == "ZENBU_script_configs")     { _config_type = "SCRIPT"; }
  if(value == "eeDB_gLyphs_autoconfigs")  { _config_type = "AUTOSAVE"; }

  if(value == "VIEW")     { _config_type = "VIEW"; }
  if(value == "TRACK")    { _config_type = "TRACK"; }
  if(value == "SCRIPT")   { _config_type = "SCRIPT"; }
  if(value == "AUTOSAVE") { _config_type = "AUTOSAVE"; }
}

long  EEDB::Configuration::access_count() { return _access_count; }
void  EEDB::Configuration::access_count(long value) { _access_count = value; }

string  EEDB::Configuration::display_name() { 
  EEDB::Metadata *md;
  if((md = metadataset()->find_metadata("eedb:display_name", ""))) { return md->data(); }
  if((md = metadataset()->find_metadata("configname", ""))) { return md->data(); }
  if((md = metadataset()->find_metadata("title", ""))) { return md->data(); }
  if((md = metadataset()->find_metadata("script_name", ""))) { return md->data(); }
  if((md = metadataset()->find_metadata("name", ""))) { return md->data(); }
  return "";
}

string  EEDB::Configuration::description() { 
  EEDB::Metadata *md;
  if((md = metadataset()->find_metadata("description", ""))) { return md->data(); }
  return "";
}

string  EEDB::Configuration::configXML() { 
  EEDB::Metadata *md = metadataset()->find_metadata("configXML", "");
  if(md) { return md->data(); }
  return "";
}


EEDB::MetadataSet*  EEDB::Configuration::metadataset() {
  if(_metadataset == NULL) { _metadataset = new EEDB::MetadataSet; }
  if(_database == NULL) { return _metadataset; }

  if(!_symbols_loaded) {
    vector<DBObject*> symbols = EEDB::Symbol::fetch_all_by_configuration_id(_database, _primary_db_id);
    _metadataset->add_metadata(symbols);
    _symbols_loaded = true;
  }
  if(!_mdata_loaded) {
    vector<DBObject*> mdata = EEDB::Metadata::fetch_all_by_configuration_id(_database, _primary_db_id);
    _metadataset->add_metadata(mdata);
    _mdata_loaded = true;;
  }
  //_database->disconnect();

  //_metadataset->remove_metadata_like("eedb:owner_OpenID", "");  //old system
  //_metadataset->remove_metadata_like("eedb:owner_nickname", "");  //old system
  //_metadataset->remove_duplicates();
  return _metadataset;
}


bool  EEDB::Configuration::check_by_filter_logic(string filter_logic) {
  if(_metadataset == NULL) { _metadataset = new EEDB::MetadataSet; }
  if(_database == NULL) { return false; }

  if(!_symbols_loaded) {
    vector<DBObject*> symbols = EEDB::Symbol::fetch_all_by_configuration_id(_database, _primary_db_id);
    _metadataset->add_metadata(symbols);
    _symbols_loaded = true;
  }
  if(_metadataset->check_by_filter_logic(filter_logic)) { return true; }
  return false;
}


EEDB::User*  EEDB::Configuration::owner() {
  if(_owner) { return _owner; }
  if(_owner_user_id == -1) { return NULL; }
  if(database() == NULL) { return NULL; }

  _owner = EEDB::User::fetch_by_id(_database, _owner_user_id);
  return _owner;
}


void  EEDB::Configuration::owner(EEDB::User* value) {
  _owner = value;
  _owner_user_id = -1;
  if(_owner) { _owner_user_id = _owner->primary_id(); }
}

string  EEDB::Configuration::last_access_date_string() { 
  string str;
  if(_last_access>0) {
    time_t t_update = _last_access;
    char *ct_value = ctime(&t_update);
    if(ct_value != NULL) {
      int len = strlen(ct_value);
      if(len>0) {
        ct_value[len-1] = '\0';
        str = ct_value;
      }
    }
  }
  return str;
}

string  EEDB::Configuration::create_date_string() { 
  string str;
  if(_create_date>0) {
    time_t t_update = _create_date;
    char *ct_value = ctime(&t_update);
    if(ct_value != NULL) {
      int len = strlen(ct_value);
      if(len>0) {
        ct_value[len-1] = '\0';
        str = ct_value;
      }
    }
  }
  return str;
}


EEDB::Collaboration*  EEDB::Configuration::collaboration() {
  if(_collaboration) { return _collaboration; }
  if(_collaboration_id == -1) { return NULL; }
  if(database() == NULL) { return NULL; }
  
  _collaboration = EEDB::Collaboration::fetch_by_id(_database, _collaboration_id);
  return _collaboration;
}


//=====================================================================

map<string, EEDB::DataSource*>  EEDB::Configuration::get_all_data_sources() {
  //fprintf(stderr, "Configuration::get_all_data_sources\n");
  map<string, EEDB::DataSource*>  datasources;
  
  string config_xml = configXML();
  if(config_xml.empty()) { return datasources; }
  
  int   xml_len  = config_xml.size();
  char* xml_text = (char*)malloc(xml_len+1);
  memset(xml_text, 0, xml_len+1);
  memcpy(xml_text, config_xml.c_str(), xml_len);  
  //fprintf(stderr, "%s\n", xml_text);
  
  rapidxml::xml_document<>  doc;
  rapidxml::xml_node<>      *node1=NULL, *node2, *root_node, *track_node;
  rapidxml::xml_attribute<> *attr;
  
  doc.parse<rapidxml::parse_declaration_node | rapidxml::parse_no_data_nodes>(xml_text);
  
  root_node = doc.first_node();
  if(!root_node) { free(xml_text); return datasources; }
  
  string root_name = root_node->name();
  boost::algorithm::to_lower(root_name);
  if(root_name == string("eedbglyphstrackconfig")) {
    node1 = root_node;
  }
  if(root_name == string("eedbglyphsconfig")) {
    node1 = root_node->first_node("gLyphTracks");
  }  
  if(!node1) { return datasources; }
  //fprintf(stderr, "OK we have track config(s)\n");
  
  map<string, string> parameters;

  track_node = node1->first_node("gLyphTrack");
  while(track_node != NULL) { 
    //fprintf(stderr, "track\n");

    if((attr = track_node->first_attribute("title"))) { parameters["track_title"] = attr->value(); }
    if((attr = track_node->first_attribute("track_title"))) { parameters["track_title"] = attr->value(); }

    for(node2 = track_node->first_node("source"); node2; node2 = node2->next_sibling("source")) {
      if((attr = node2->first_attribute("id"))) {
        datasources[attr->value()] = NULL;
        //fprintf(stderr, "  %s\n", attr->value());
      }
    }

    string source_ids;
    if((node2 = track_node->first_node("source_ids"))) { source_ids = node2->value(); }    
    if((attr = track_node->first_attribute("source_ids"))) { source_ids = attr->value(); }
    
    if(!source_ids.empty()) {
      char* buf = (char*)malloc(source_ids.size()+2);
      strcpy(buf, source_ids.c_str());
      char *p1 = strtok(buf, ", \t");
      while(p1!=NULL) {
        if(strlen(p1) > 0) {
          datasources[p1] = NULL;
          //fprintf(stderr, "  %s\n", p1);
        }
        p1 = strtok(NULL, ", \t");
      }
      free(buf);
    }
    
    //TODO: need to get sources from script
    //if((node = track_node->first_node("zenbu_script")) != NULL) { parse_processing_stream(node); }
    
    track_node = track_node->next_sibling("gLyphTrack");
  }
  free(xml_text);
  
  long miss_count=0;
  map<string, EEDB::DataSource*>::iterator it1;
  for(it1=datasources.begin(); it1!=datasources.end(); it1++) {
    if((*it1).second) { continue; }
    string dbid = (*it1).first;
    EEDB::DataSource *source = EEDB::DataSource::sources_cache_get(dbid);
    if(source) { 
      source->retain();
      (*it1).second = source; 
    } else { miss_count++; }
  }
  //fprintf(stderr, "%ld missing\n", miss_count);
    
  return datasources;
}



////////////////////////////////////////////////////////////////////////

DBObject* EEDB::Configuration::create(map<string, dynadata> &row_map, Database* db) {
  EEDB::Configuration* obj = new EEDB::Configuration;
  obj->database(db);
  obj->init_from_row_map(row_map);
  return obj;
}


void  EEDB::Configuration::init_from_row_map(map<string, dynadata> &row_map) {
  _primary_db_id     = row_map["configuration_id"].i_int;
  _owner_user_id     = row_map["user_id"].i_int;
  _collaboration_id  = row_map["collaboration_id"].i_int;
  _uuid              = row_map["uuid"].i_string;
  _config_type       = row_map["config_type"].i_string;
  _access_count      = row_map["access_count"].i_int;
  
  if(row_map["last_access"].type == MQDB::STRING) {
    string date_string = row_map["last_access"].i_string;
    _last_access = MQDB::seconds_from_epoch(date_string);
  }
  if(row_map["last_access"].type == MQDB::TIMESTAMP) {
    _last_access = row_map["last_access"].i_timestamp;
  }

  if(row_map["create_date"].type == MQDB::STRING) {
    string date_string = row_map["create_date"].i_string;
    _create_date = MQDB::seconds_from_epoch(date_string);
  }
  if(row_map["create_date"].type == MQDB::TIMESTAMP) {
    _create_date = row_map["create_date"].i_timestamp;
  }
}


EEDB::Configuration*  EEDB::Configuration::fetch_by_id(MQDB::Database *db, long int id) {
  const char *sql = "SELECT * FROM configuration WHERE configuration_id=?";
  return (EEDB::Configuration*) MQDB::fetch_single(EEDB::Configuration::create, db, sql, "d", id);
}


vector<DBObject*> EEDB::Configuration::fetch_all(MQDB::Database *db) {
  const char *sql = "SELECT * FROM configuration";
  return MQDB::fetch_multiple(EEDB::Configuration::create, db, sql, "");
}


EEDB::Configuration*  EEDB::Configuration::fetch_by_uuid(MQDB::Database *db, string uuid) {
  const char *sql = "SELECT * FROM configuration WHERE uuid=?";
  return (EEDB::Configuration*) MQDB::fetch_single(EEDB::Configuration::create, db, sql, "s", uuid.c_str());
}


EEDB::Configuration*  EEDB::Configuration::fetch_by_uuid(MQDB::Database *db, string uuid, EEDB::User* user) {
  long user_id = -1;
  if(user) { user_id = user->primary_id(); }
  
  const char *sql = "SELECT * from \
                     (SELECT configuration.* from configuration JOIN collaboration USING(collaboration_id ) \
                        JOIN collaboration_2_user USING(collaboration_id ) WHERE collaboration_2_user.user_id =? \
                      UNION SELECT configuration.* from configuration left join collaboration USING(collaboration_id) \
                        WHERE collaboration.uuid=\"public\" OR collaboration.uuid =\"curated\" OR collaboration.open_to_public=\"y\" \
                        OR config_type =\"AUTOSAVE\" OR user_id =? \
                     )t WHERE uuid=?";
  return (EEDB::Configuration*) MQDB::fetch_single(EEDB::Configuration::create, db, sql, 
                                                   "dds", user_id, user_id, uuid.c_str());
}


vector<DBObject*>  EEDB::Configuration::fetch_all_by_type(MQDB::Database *db, string config_type, EEDB::User* user) {
  vector<DBObject*> t_result;
  if(!db) { return t_result; }
  
  if(user) {
    long user_id = user->primary_id();
    string sql = "SELECT * FROM (SELECT configuration.* from configuration JOIN collaboration USING(collaboration_id ) JOIN collaboration_2_user USING(collaboration_id ) WHERE collaboration_2_user.user_id =? UNION SELECT * from configuration WHERE user_id =? UNION SELECT configuration.* from configuration JOIN collaboration USING(collaboration_id) WHERE (collaboration.uuid='public' OR collaboration.uuid ='curated' OR collaboration.open_to_public='y'))t";
    if(config_type.empty()) {
      sql += " WHERE config_type !='AUTOSAVE'";
    } else {
      sql += " WHERE config_type ='" + config_type+"'";
    }
    t_result = MQDB::fetch_multiple(EEDB::Configuration::create, db, sql.c_str(), "dd", user_id, user_id);
  } else {
    if(config_type.empty()) {
      const char* sql = "SELECT configuration.* from configuration join collaboration USING(collaboration_id) \
                           WHERE (collaboration.uuid='curated' OR collaboration.open_to_public='y')";
      t_result = MQDB::fetch_multiple(EEDB::Configuration::create, db, sql, "");
    } else {
      const char* sql = "SELECT configuration.* from configuration join collaboration USING(collaboration_id) \
                           WHERE (collaboration.uuid='curated' OR collaboration.open_to_public='y') AND config_type=?";
      t_result = MQDB::fetch_multiple(EEDB::Configuration::create, db, sql, "s", config_type.c_str());
    }
  }
  return t_result;
}


vector<DBObject*>  EEDB::Configuration::fetch_by_uuid_list(MQDB::Database *db, EEDB::User* user, string uuid_list) {
  vector<DBObject*> t_result;
  if(!db) { return t_result; }
  
  string uuids_str;
  char *tok;
  char *buffer = (char*)malloc(uuid_list.size()+3);
  strcpy(buffer, uuid_list.c_str());
  tok =strtok(buffer, " \t,");
  while(tok!=NULL) {
    if(strlen(tok) >0) { 
      if(!uuids_str.empty()) { uuids_str += ","; }
      uuids_str += string("\"") + string(tok) + "\"";
    }
    tok =strtok(NULL, " \t,");
  }
  free(buffer);
  if(uuids_str.empty()) { return t_result; }
  
  string sql;
  if(user) { 
    long user_id = user->primary_id();    
    char uid_buf[64];
    snprintf(uid_buf, 63, "%ld", user_id);
    sql =  "SELECT configuration.* from configuration JOIN collaboration USING(collaboration_id ) ";
    sql +=   "JOIN collaboration_2_user USING(collaboration_id ) ";
    sql +=   "WHERE collaboration_2_user.user_id="+string(uid_buf)+" AND configuration.uuid IN("+uuids_str+") ";
    sql += "UNION SELECT * from configuration WHERE user_id="+string(uid_buf)+" AND configuration.uuid IN("+uuids_str+") ";
    sql += "UNION SELECT configuration.* from configuration JOIN collaboration USING(collaboration_id) ";
    sql +=   "WHERE (collaboration.uuid='public' OR collaboration.uuid ='curated' OR collaboration.open_to_public=\"y\") ";
    sql +=   "AND configuration.uuid IN("+uuids_str+")";
  } else {
    sql =  "SELECT configuration.* FROM configuration JOIN collaboration USING(collaboration_id) ";
    sql +=   "WHERE (collaboration.uuid='public' OR collaboration.uuid ='curated' OR collaboration.open_to_public=\"y\") AND configuration.uuid IN("+uuids_str+")";
  }  

  //fprintf(stderr, "%s\n", sql.c_str());
  t_result = MQDB::fetch_multiple(EEDB::Configuration::create, db, sql.c_str(), "");
  return t_result;
}


vector<DBObject*>  EEDB::Configuration::fetch_all_with_keywords(MQDB::Database *db, string config_type, 
                                                                EEDB::User* user, vector<string> &keywords) {
  // fetch_all_with_keywords
  //    used for feature metadata filtering, generates list of potential configurations to post process
  vector<DBObject*>  t_result;
  if(keywords.empty()) { return t_result; }
    
  //TRY new approach. first simple join on type/keywords, then post filter for security
  //select distinct configuration.configuration_id from configuration join configuration_2_symbol using(configuration_id) join symbol using(symbol_id)  where (sym_value like "hg19%" or sym_value like "trackset%") and configuration.config_type ="view";
  
  string sql = "SELECT configuration.* FROM configuration ";
  
  //filter on keywords and config_type with simple join
  sql += "JOIN (SELECT distinct configuration.configuration_id FROM configuration \
                JOIN configuration_2_symbol using(configuration_id) \
                JOIN symbol using(symbol_id) WHERE "; //config_type =\""+config_type+"\" AND (";
  if(!config_type.empty()) {
    sql += " config_type =\""+config_type+"\" AND (";
  } else {
    sql += " config_type !='AUTOSAVE' AND (";
  }
  
  vector<string>::iterator  it1;
  for(it1=keywords.begin(); it1!=keywords.end(); it1++) { //loop on keywords
    if(it1 != keywords.begin()) { sql += " OR "; }
    sql += "sym_value like \"" + (*it1) + "%\""; 
  }  
  sql += ") )c2 USING(configuration_id) ";

  
  //then add in the user security access filtering
  if(user) { 
    long user_id = user->primary_id();
    char buffer[64];
    snprintf(buffer, 63, "%ld", user_id);
    sql += "JOIN (SELECT configuration_id FROM configuration JOIN collaboration_2_user USING(collaboration_id) ";
    sql +=    "WHERE collaboration_2_user.user_id=" + string(buffer) + " ";
    sql += "UNION SELECT configuration_id FROM configuration WHERE user_id=" + string(buffer) + " ";
    sql += "UNION SELECT configuration_id FROM configuration JOIN collaboration USING(collaboration_id) ";
    sql +=    "WHERE (collaboration.uuid='public' OR collaboration.uuid ='curated' OR collaboration.open_to_public=\"y\"))cl1 ";
    sql += "USING(configuration_id)";
  } else {
    sql += "JOIN (SELECT distinct configuration_id FROM configuration JOIN collaboration USING(collaboration_id) ";
    //sql += "WHERE (collaboration.uuid='curated' OR collaboration.uuid='public'))cl1 ";
    sql += "WHERE (collaboration.uuid='curated' OR collaboration.open_to_public='y'))cl1 ";
    sql += "USING(configuration_id)";
  }  

  //fprintf(stderr, "%s\n", sql.c_str());
  return MQDB::fetch_multiple(EEDB::Configuration::create, db, sql.c_str(), "");
}


vector<DBObject*> EEDB::Configuration::fetch_by_metadata_search(MQDB::Database *db, string config_type, 
                                                                           EEDB::User* user, string filter_logic) {
  vector<DBObject*> t_result, filter_results;
  if(!db) { return t_result; }
    
  //if filter is empty fetch all features
  if(filter_logic.empty()) {
    return EEDB::Configuration::fetch_all_by_type(db, config_type, user);
  }

  //now extract critical keywords, pre-stream potential features
  //then followup with full logic filtering for actual matches
  vector<string>  keywords;
  bool add_next_tok = true;
  char *tok;
  char *buffer = (char*)malloc(filter_logic.size()+3);
  strcpy(buffer, filter_logic.c_str());
  tok =strtok(buffer, " \t()");
  while(tok!=NULL) {
    bool ok=true;
    if(tok[0] == '!') { ok = false; }
    if(strcmp(tok, "and")==0) { ok = false; }
    if(strcmp(tok, "or")==0) { ok = false; add_next_tok=true; }
    if(strlen(tok) < 3) { ok = false; }
    if(ok and add_next_tok) {
      keywords.push_back(tok);
      add_next_tok = false;
    }
    tok =strtok(NULL, " \t()");
  }
  free(buffer);
  
  t_result = EEDB::Configuration::fetch_all_with_keywords(db, config_type, user, keywords);
  //t_result = EEDB::Configuration::fetch_all_by_type(db, config_type, user);

  vector<MQDB::DBObject*>::iterator it2;
  for(it2 = t_result.begin(); it2 != t_result.end(); it2++) {
    EEDB::Configuration *config = (EEDB::Configuration*)(*it2);
    if(config->check_by_filter_logic(filter_logic)) { filter_results.push_back(config); }
    else { config->release(); }
  }

  return filter_results;
}



//////////////////////////////////////////////////////////////////////////////////////////////////
//
// DBObject override methods
//
//////////////////////////////////////////////////////////////////////////////////////////////////

bool EEDB::Configuration::check_exists_db(MQDB::Database *db) {
  if(db==NULL) { return false; }
  
  dynadata value = db->fetch_col_value("SELECT configuration_id FROM configuration WHERE uuid=?", "s", _uuid.c_str());
  if(value.type != MQDB::INT) { return false; }
  
  _primary_db_id = value.i_int;
  database(db);
  _peer_uuid = db->uuid();
  _db_id.clear();  
  return true;
}


bool EEDB::Configuration::store(MQDB::Database *db) {
  if(db==NULL) { return false; }
  if(check_exists_db(db)) { return true; }
    
  db->do_sql("INSERT ignore INTO configuration (user_id, uuid, config_type, access_count, create_date) VALUES(?,?,?,?,NOW())", 
             "dssd", _owner_user_id, _uuid.c_str(), _config_type.c_str(), _access_count);
             
  if(!check_exists_db(db)) { return false; }

  //now do the symbols and metadata  
  store_metadata();
  
  return true;
}


bool EEDB::Configuration::store_metadata() {
  if(database() == NULL) { return false; }
  if(primary_id() == -1) { return false; }

  MQDB::Database *db = database();
  
  vector<EEDB::Metadata*> mdlist = metadataset()->metadata_list();
  for(unsigned int i=0; i<mdlist.size(); i++) {
    EEDB::Metadata *md = mdlist[i];
    if(md->classname() == EEDB::Symbol::class_name) {
      EEDB::Symbol *sym = (EEDB::Symbol*)md;
      if(!sym->check_exists_db(db)) { sym->store(db); }
      sym->store_link_to_configuration(this);
    } else {
      md->store(db);
      md->store_link_to_configuration(this);
    }
  }
  return true;
}


bool EEDB::Configuration::update_metadata() {
  if(database() == NULL) { return false; }
  if(primary_id() == -1) { return false; }
  
  MQDB::Database *db = database();
  
  //unlink all old metadata
  db->do_sql("DELETE FROM configuration_2_metadata WHERE configuration_id=?", "d", primary_id());
  db->do_sql("DELETE FROM configuration_2_symbol   WHERE configuration_id=?", "d", primary_id());

  //store again
  store_metadata();
  return true;
}


bool EEDB::Configuration::link_to_collaboration(EEDB::Collaboration* collab) {
  if(database() == NULL) { return false; }
  if(primary_id() == -1) { return false; }

  long collab_id = -1;
  if(collab) { collab_id = collab->primary_id(); }
  database()->do_sql("UPDATE configuration SET collaboration_id=? WHERE configuration_id=?", 
                     "dd", collab_id, primary_id());  

  _collaboration = collab;
  _collaboration_id = collab_id;

  return true;
}


void EEDB::Configuration::update_usage() {
  if(database() == NULL) { return; }
  if(primary_id() == -1) { return; }
  
  database()->do_sql("UPDATE configuration SET access_count=access_count+1 WHERE configuration_id=?",
             "d", primary_id());  
}


bool EEDB::Configuration::delete_from_db() {
  if(database() == NULL) { return false; }
  if(primary_id() == -1) { return false; }
  
  MQDB::Database *db = database();
  
  db->do_sql("DELETE FROM configuration_2_metadata WHERE configuration_id=?", "d", primary_id());
  db->do_sql("DELETE FROM configuration_2_symbol   WHERE configuration_id=?", "d", primary_id());
  db->do_sql("DELETE FROM configuration WHERE configuration_id=?", "d", primary_id());
  
  return true;
}

