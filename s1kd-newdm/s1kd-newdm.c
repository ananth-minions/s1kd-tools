#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#include <libxml/tree.h>

#include "templates.h"

#define MAX_MODEL_IDENT_CODE		14	+ 2
#define MAX_SYSTEM_DIFF_CODE		 4	+ 2
#define MAX_SYSTEM_CODE			 3	+ 2
#define MAX_SUB_SYSTEM_CODE		 1	+ 2
#define MAX_SUB_SUB_SYSTEM_CODE		 1	+ 2
#define MAX_ASSY_CODE			 4	+ 2
#define MAX_DISASSY_CODE		 2	+ 2
#define MAX_DISASSY_CODE_VARIANT	 1	+ 2
#define MAX_INFO_CODE			 3	+ 2
#define MAX_INFO_CODE_VARIANT		 1	+ 2
#define MAX_ITEM_LOCATION_CODE		 1	+ 2
#define MAX_LANGUAGE_ISO_CODE		 3	+ 2
#define MAX_COUNTRY_ISO_CODE		 2	+ 2
#define MAX_ISSUE_NUMBER		 3	+ 2
#define MAX_IN_WORK			 2	+ 2
#define MAX_SECURITY_CLASSIFICATION	 2	+ 2

#define MAX_DATAMODULE_CODE 256

#define MAX_ENTERPRISE_NAME 256
#define MAX_TECH_NAME 256
#define MAX_INFO_NAME 256

#define ERR_PREFIX "s1kd-newdm: ERROR: "

#define EXIT_DM_EXISTS 1
#define EXIT_UNKNOWN_DMTYPE 2

void prompt(const char *prompt, char *str, int n)
{
	if (strcmp(str, "") == 0) {
		printf("%s: ", prompt);
		fgets(str, n, stdin);
		strtok(str, "\n");
	} else {
		char temp[512] = "";

		printf("%s [%s]: ", prompt, str);
		fgets(temp, n, stdin);
		*(strchr(temp, '\n')) = '\0';
		if (strcmp(temp, "") != 0) {
			strncpy(str, temp, n);
		}
	}
}

xmlNode *find_child(xmlNode *parent, const char *name)
{
	xmlNode *cur;

	for (cur = parent->children; cur; cur = cur->next) {
		if (strcmp((char *) cur->name, name) == 0) {
			return cur;
		}
	}

	return NULL;
}

void show_help(void)
{
	puts("Usage: s1kd-newdm [options]");
	puts("");
	puts("Options:");
	puts("  -p	Prompt the user for each value");
	puts("");
	puts("In addition, the following pieces of meta data can be set:");
	puts("  -#	Data module code");
	puts("  -L	Language ISO code");
	puts("  -C	Country ISO code");
	puts("  -n	Issue number");
	puts("  -w	Inwork issue");
	puts("  -c	Security classification");
	puts("  -r	Responsible partner company enterprise name");
	puts("  -o	Originator enterprise name");
	puts("  -t	Tech name");
	puts("  -i	Info name");
	puts("  -T	DM type (descript, proced, frontmatter, brex, brdoc)");
}

int main(int argc, char **argv)
{
	char modelIdentCode[MAX_MODEL_IDENT_CODE] = "";
	char systemDiffCode[MAX_SYSTEM_DIFF_CODE] = "";
	char systemCode[MAX_SYSTEM_CODE] = "";
	char subSystemCode[MAX_SUB_SYSTEM_CODE] = "";
	char subSubSystemCode[MAX_SUB_SUB_SYSTEM_CODE] = "";
	char assyCode[MAX_ASSY_CODE] = "";
	char disassyCode[MAX_DISASSY_CODE] = "";
	char disassyCodeVariant[MAX_DISASSY_CODE_VARIANT] = "";
	char infoCode[MAX_INFO_CODE] = "";
	char infoCodeVariant[MAX_INFO_CODE_VARIANT] = "";
	char itemLocationCode[MAX_ITEM_LOCATION_CODE] = "";

	char languageIsoCode[MAX_LANGUAGE_ISO_CODE] = "";
	char countryIsoCode[MAX_COUNTRY_ISO_CODE] = "";

	char securityClassification[MAX_SECURITY_CLASSIFICATION] = "";

	char issueNumber[MAX_ISSUE_NUMBER] = "";
	char inWork[MAX_IN_WORK] = "";

	char responsiblePartnerCompany_enterpriseName[MAX_ENTERPRISE_NAME] = "";
	char originator_enterpriseName[MAX_ENTERPRISE_NAME] = "";

	char techName_content[MAX_TECH_NAME] = "";
	char infoName_content[MAX_INFO_NAME] = "";

	time_t now;
	struct tm *local;
	int year, month, day;
	char year_s[5], month_s[3], day_s[3];

	char dmtype[32] = "";

	char dmc[MAX_DATAMODULE_CODE];

	xmlDocPtr dm;
	xmlNode *dmodule;
	xmlNode *identAndStatusSection;
	xmlNode *dmAddress;
	xmlNode *dmIdent;
	xmlNode *dmCode;
	xmlNode *language;
	xmlNode *issueInfo;
	xmlNode *dmAddressItems;
	xmlNode *issueDate;
	xmlNode *dmStatus;
	xmlNode *security;
	xmlNode *dmTitle;
	xmlNode *techName;
	xmlNode *infoName;
	xmlNode *responsiblePartnerCompany;
	xmlNode *originator;
	xmlNode *enterpriseName;

	FILE *defaults;
	char default_line[1024];
	char *def_key;
	char *def_val;

	char defaults_fname[256] = "defaults";
	char dmtypes_fname[256] = "dmtypes";

	int i;
	int c;

	bool showprompts = false;
	char dmcode[256] = "";
	bool skipdmc = false;

	while ((c = getopt(argc, argv, "pd:D:L:C:n:w:c:r:o:t:i:T:#:h?")) != -1) {
		switch (c) {
			case 'p': showprompts = true; break;
			case 'd': strcpy(defaults_fname, optarg); break;
			case 'D': strcpy(dmtypes_fname, optarg); break;
			case 'L': strcpy(languageIsoCode, optarg); break;
			case 'C': strcpy(countryIsoCode, optarg); break;
			case 'n': strcpy(issueNumber, optarg); break;
			case 'w': strcpy(inWork, optarg); break;
			case 'c': strcpy(securityClassification, optarg); break;
			case 'r': strcpy(responsiblePartnerCompany_enterpriseName, optarg); break;
			case 'o': strcpy(originator_enterpriseName, optarg); break;
			case 't': strcpy(techName_content, optarg); break;
			case 'i': strcpy(infoName_content, optarg); break;
			case 'T': strcpy(dmtype, optarg); break;
			case '#': strcpy(dmcode, optarg); skipdmc = true; break;
			case 'h':
			case '?': show_help(); exit(0);
		}
	}

	defaults = fopen(defaults_fname, "r");

	if (defaults) {
		while (fgets(default_line, 1024, defaults)) {
			def_key = strtok(default_line, "\t ");
			def_val = strtok(NULL, "\t\n");

			if (strcmp(def_key, "modelIdentCode") == 0 && strcmp(modelIdentCode, "") == 0)
				strcpy(modelIdentCode, def_val);
			else if (strcmp(def_key, "systemDiffCode") == 0 && strcmp(systemDiffCode, "") == 0)
				strcpy(systemDiffCode, def_val);
			else if (strcmp(def_key, "systemCode") == 0 && strcmp(systemCode, "") == 0)
				strcpy(systemCode, def_val);
			else if (strcmp(def_key, "subSystemCode") == 0 && strcmp(subSystemCode, "") == 0)
				strcpy(subSystemCode, def_val);
			else if (strcmp(def_key, "subSubSystemCode") == 0 && strcmp(subSubSystemCode, "") == 0)
				strcpy(subSubSystemCode, def_val);
			else if (strcmp(def_key, "assyCode") == 0 && strcmp(assyCode, "") == 0)
				strcpy(assyCode, def_val);
			else if (strcmp(def_key, "disassyCode") == 0 && strcmp(disassyCode, "") == 0)
				strcpy(disassyCode, def_val);
			else if (strcmp(def_key, "disassyCodeVariant") == 0 && strcmp(disassyCodeVariant, "") == 0)
				strcpy(disassyCodeVariant, def_val);
			else if (strcmp(def_key, "infoCode") == 0 && strcmp(infoCode, "") == 0)
				strcpy(infoCode, def_val);
			else if (strcmp(def_key, "infoCodeVariant") == 0 && strcmp(infoCodeVariant, "") == 0)
				strcpy(infoCodeVariant, def_val);
			else if (strcmp(def_key, "itemLocationCode") == 0 && strcmp(itemLocationCode, "") == 0)
				strcpy(itemLocationCode, def_val);
			else if (strcmp(def_key, "languageIsoCode") == 0 && strcmp(languageIsoCode, "") == 0)
				strcpy(languageIsoCode, def_val);
			else if (strcmp(def_key, "countryIsoCode") == 0 && strcmp(countryIsoCode, "") == 0)
				strcpy(countryIsoCode, def_val);
			else if (strcmp(def_key, "issueNumber") == 0 && strcmp(issueNumber, "") == 0)
				strcpy(issueNumber, def_val);
			else if (strcmp(def_key, "inWork") == 0 && strcmp(inWork, "") == 0)
				strcpy(inWork, def_val);
			else if (strcmp(def_key, "securityClassification") == 0 && strcmp(securityClassification, "") == 0)
				strcpy(securityClassification, def_val);
			else if (strcmp(def_key, "responsiblePartnerCompany") == 0 && strcmp(responsiblePartnerCompany_enterpriseName, "") == 0)
				strcpy(responsiblePartnerCompany_enterpriseName, def_val);
			else if (strcmp(def_key, "originator") == 0 && strcmp(originator_enterpriseName, "") == 0)
				strcpy(originator_enterpriseName, def_val);
			else if (strcmp(def_key, "techName") == 0 && strcmp(techName_content, "") == 0)
				strcpy(techName_content, def_val);
			else if (strcmp(def_key, "infoName") == 0 && strcmp(infoName_content, "") == 0)
				strcpy(infoName_content, def_val);
		}

		fclose(defaults);
	}

	if (strcmp(dmcode, "") != 0) {
		char *subAndSubSub;
		char *disassyCodeAndVariant;
		char *infoCodeAndVariant;

		strcpy(modelIdentCode, strtok(dmcode, "-"));
		strcpy(systemDiffCode, strtok(NULL, "-"));
		strcpy(systemCode, strtok(NULL, "-"));
		subAndSubSub = strtok(NULL, "-");
		strcpy(assyCode, strtok(NULL, "-"));
		disassyCodeAndVariant = strtok(NULL, "-");
		infoCodeAndVariant = strtok(NULL, "-");
		strcpy(itemLocationCode, strtok(NULL, ""));

		strncpy(subSystemCode, subAndSubSub, 1);
		strncpy(subSubSystemCode, subAndSubSub + 1, 1);
		strncpy(disassyCode, disassyCodeAndVariant, 2);
		strcpy(disassyCodeVariant, disassyCodeAndVariant + 2);
		strncpy(infoCode, infoCodeAndVariant, 3);
		strcpy(infoCodeVariant, infoCodeAndVariant + 3);
	}

	defaults = fopen(dmtypes_fname, "r");

	if (defaults) {
		while (fgets(default_line, 1024, defaults)) {
			def_key = strtok(default_line, "\t ");
			def_val = strtok(NULL, "\t\n");

			if (strcmp(def_key, infoCode) == 0)
				strcpy(dmtype, def_val);
		}

		fclose(defaults);
	}

	if (showprompts) {
		if (!skipdmc) {
			prompt("Model identification code", modelIdentCode, MAX_MODEL_IDENT_CODE);
			prompt("System difference code", systemDiffCode, MAX_SYSTEM_DIFF_CODE);
			prompt("System code", systemCode, MAX_SYSTEM_CODE);
			prompt("Sub-system code", subSystemCode, MAX_SUB_SYSTEM_CODE);
			prompt("Sub-sub-system code", subSubSystemCode, MAX_SUB_SUB_SYSTEM_CODE);
			prompt("Assembly code", assyCode, MAX_ASSY_CODE);
			prompt("Disassembly code", disassyCode, MAX_DISASSY_CODE);
			prompt("Disassembly code variant", disassyCodeVariant, MAX_DISASSY_CODE_VARIANT);
			prompt("Information code", infoCode, MAX_INFO_CODE);
			prompt("Information code variant", infoCodeVariant, MAX_INFO_CODE_VARIANT);
			prompt("Item location code", itemLocationCode, MAX_ITEM_LOCATION_CODE);
		}
		prompt("Language ISO code", languageIsoCode, MAX_LANGUAGE_ISO_CODE);
		prompt("Country ISO code", countryIsoCode, MAX_COUNTRY_ISO_CODE);
		prompt("Issue number", issueNumber, MAX_ISSUE_NUMBER);
		prompt("In-work issue", inWork, MAX_IN_WORK);
		prompt("Security classification", securityClassification, MAX_SECURITY_CLASSIFICATION);
		prompt("Responsible partner company", responsiblePartnerCompany_enterpriseName, MAX_ENTERPRISE_NAME);
		prompt("Originator", originator_enterpriseName, MAX_ENTERPRISE_NAME);
		prompt("Tech name", techName_content, MAX_TECH_NAME);
		prompt("Info name", infoName_content, MAX_INFO_NAME);

		prompt("DM type", dmtype, 32);
	}

	if (strcmp(dmtype, "descript") == 0)
		dm = xmlReadMemory((const char *)templates_descript_xml, templates_descript_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "proced") == 0)
		dm = xmlReadMemory((const char *) templates_proced_xml, templates_proced_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "frontmatter") == 0)
		dm = xmlReadMemory((const char *) templates_frontmatter_xml, templates_frontmatter_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "brex") == 0)
		dm = xmlReadMemory((const char *) templates_brex_xml, templates_brex_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "brdoc") == 0)
		dm = xmlReadMemory((const char *) templates_brdoc_xml, templates_brdoc_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "appliccrossreftable") == 0)
		dm = xmlReadMemory((const char *) templates_appliccrossreftable_xml, templates_appliccrossreftable_xml_len, NULL, NULL, 0);
	else if (strcmp(dmtype, "comrep") == 0)
		dm = xmlReadMemory((const char *) templates_comrep_xml, templates_comrep_xml_len, NULL, NULL, 0);
	else {
		fprintf(stderr, "ERROR: Unknown dmtype %s\n", dmtype);
		exit(EXIT_UNKNOWN_DMTYPE);
	}

	dmodule = xmlDocGetRootElement(dm);
	identAndStatusSection = find_child(dmodule, "identAndStatusSection");
	dmAddress = find_child(identAndStatusSection, "dmAddress");
	dmIdent = find_child(dmAddress, "dmIdent");
	dmCode = find_child(dmIdent, "dmCode");
	language = find_child(dmIdent, "language");
	issueInfo = find_child(dmIdent, "issueInfo");
	dmAddressItems = find_child(dmAddress, "dmAddressItems");
	issueDate = find_child(dmAddressItems, "issueDate");
	dmStatus = find_child(identAndStatusSection, "dmStatus");
	security = find_child(dmStatus, "security");
	dmTitle = find_child(dmAddressItems, "dmTitle");
	techName = find_child(dmTitle, "techName");
	infoName = find_child(dmTitle, "infoName");

	xmlSetProp(dmCode, (xmlChar *) "modelIdentCode", (xmlChar *) modelIdentCode);
	xmlSetProp(dmCode, (xmlChar *) "systemDiffCode", (xmlChar *) systemDiffCode);
	xmlSetProp(dmCode, (xmlChar *) "systemCode", (xmlChar *) systemCode);
	xmlSetProp(dmCode, (xmlChar *) "subSystemCode", (xmlChar *) subSystemCode);
	xmlSetProp(dmCode, (xmlChar *) "subSubSystemCode", (xmlChar *) subSubSystemCode);
	xmlSetProp(dmCode, (xmlChar *) "assyCode", (xmlChar *) assyCode);
	xmlSetProp(dmCode, (xmlChar *) "disassyCode", (xmlChar *) disassyCode);
	xmlSetProp(dmCode, (xmlChar *) "disassyCodeVariant", (xmlChar *) disassyCodeVariant);
	xmlSetProp(dmCode, (xmlChar *) "infoCode", (xmlChar *) infoCode);
	xmlSetProp(dmCode, (xmlChar *) "infoCodeVariant", (xmlChar *) infoCodeVariant);
	xmlSetProp(dmCode, (xmlChar *) "itemLocationCode", (xmlChar *) itemLocationCode);

	xmlSetProp(language, (xmlChar *) "languageIsoCode", (xmlChar *) languageIsoCode);
	xmlSetProp(language, (xmlChar *) "countryIsoCode", (xmlChar *) countryIsoCode);

	xmlSetProp(issueInfo, (xmlChar *) "issueNumber", (xmlChar *) issueNumber);
	xmlSetProp(issueInfo, (xmlChar *) "inWork", (xmlChar *) inWork);

	time(&now);
	local = localtime(&now);
	year = local->tm_year + 1900;
	month = local->tm_mon + 1;
	day = local->tm_mday;

	sprintf(day_s, "%.2d", day);
	sprintf(month_s, "%.2d", month);
	sprintf(year_s, "%d", year);

	xmlSetProp(issueDate, (xmlChar *) "year", (xmlChar *) year_s);
	xmlSetProp(issueDate, (xmlChar *) "month", (xmlChar *) month_s);
	xmlSetProp(issueDate, (xmlChar *) "day", (xmlChar *) day_s);

	xmlSetProp(security, (xmlChar *) "securityClassification", (xmlChar *) securityClassification);

	xmlNodeAddContent(techName, (xmlChar *) techName_content);

	if (strcmp(infoName_content, "\n") == 0) {
		xmlUnlinkNode(infoName);
		xmlFreeNode(infoName);
	} else {
		xmlNodeAddContent(infoName, (xmlChar *) infoName_content);
	}

	responsiblePartnerCompany = find_child(dmStatus, "responsiblePartnerCompany");
	enterpriseName = find_child(responsiblePartnerCompany, "enterpriseName");
	xmlNodeAddContent(enterpriseName, (xmlChar *) responsiblePartnerCompany_enterpriseName);

	originator = find_child(dmStatus, "originator");
	enterpriseName = find_child(originator, "enterpriseName");
	xmlNodeAddContent(enterpriseName, (xmlChar *) originator_enterpriseName);

	for (i = 0; languageIsoCode[i]; ++i) languageIsoCode[i] = toupper(languageIsoCode[i]);

	snprintf(dmc, MAX_DATAMODULE_CODE,
		"DMC-%s-%s-%s-%s%s-%s-%s%s-%s%s-%s_%s-%s_%s-%s.XML",
		modelIdentCode,
		systemDiffCode,
		systemCode,
		subSystemCode,
		subSubSystemCode,
		assyCode,
		disassyCode,
		disassyCodeVariant,
		infoCode,
		infoCodeVariant,
		itemLocationCode,
		issueNumber,
		inWork,
		languageIsoCode,
		countryIsoCode);

	if (access(dmc, F_OK) != -1) {
		fprintf(stderr, ERR_PREFIX "Data module already exists.\n");
		exit(EXIT_DM_EXISTS);
	}

	xmlSaveFormatFile(dmc, dm, 1);

	xmlFreeDoc(dm);

	return 0;
}