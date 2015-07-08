/*
 * Copyright 2015, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include "Conditions.h"

#include <stdio.h>

#include <Entry.h>
#include <ObjectList.h>
#include <Message.h>
#include <StringList.h>

#include "Utility.h"


class ConditionContainer : public Condition {
protected:
								ConditionContainer(const BMessage& args);
								ConditionContainer();

public:
			void				AddCondition(Condition* condition);

	virtual	bool				IsConstant(ConditionContext& context) const;

protected:
			void				ToString(BString& string) const;

protected:
			BObjectList<Condition> fConditions;
};


class AndCondition : public ConditionContainer {
public:
								AndCondition(const BMessage& args);
								AndCondition();

	virtual	bool				Test(ConditionContext& context) const;
	virtual	BString				ToString() const;
};


class OrCondition : public ConditionContainer {
public:
								OrCondition(const BMessage& args);

	virtual	bool				Test(ConditionContext& context) const;
	virtual	bool				IsConstant(ConditionContext& context) const;

	virtual	BString				ToString() const;
};


class NotCondition : public ConditionContainer {
public:
								NotCondition(const BMessage& args);
								NotCondition();

	virtual	bool				Test(ConditionContext& context) const;
	virtual	BString				ToString() const;
};


class SafeModeCondition : public Condition {
public:
	virtual	bool				Test(ConditionContext& context) const;
	virtual	bool				IsConstant(ConditionContext& context) const;

	virtual	BString				ToString() const;
};


class ReadOnlyCondition : public Condition {
public:
								ReadOnlyCondition(const BMessage& args);

	virtual	bool				Test(ConditionContext& context) const;
	virtual	bool				IsConstant(ConditionContext& context) const;

	virtual	BString				ToString() const;

private:
			BString				fPath;
	mutable	bool				fIsReadOnly;
	mutable	bool				fTestPerformed;
};


class FileExistsCondition : public Condition {
public:
								FileExistsCondition(const BMessage& args);

	virtual	bool				Test(ConditionContext& context) const;
	virtual	BString				ToString() const;

private:
			BStringList			fPaths;
};


static Condition*
create_condition(const char* name, const BMessage& args)
{
	if (strcmp(name, "and") == 0)
		return new AndCondition(args);
	if (strcmp(name, "or") == 0)
		return new OrCondition(args);
	if (strcmp(name, "not") == 0)
		return new NotCondition(args);

	if (strcmp(name, "safemode") == 0)
		return new SafeModeCondition();
	if (strcmp(name, "read_only") == 0)
		return new ReadOnlyCondition(args);
	if (strcmp(name, "file_exists") == 0)
		return new FileExistsCondition(args);

	return NULL;
}


// #pragma mark -


Condition::Condition()
{
}


Condition::~Condition()
{
}


bool
Condition::IsConstant(ConditionContext& context) const
{
	return false;
}


// #pragma mark -


ConditionContainer::ConditionContainer(const BMessage& args)
	:
	fConditions(10, true)
{
	char* name;
	type_code type;
	int32 count;
	for (int32 index = 0; args.GetInfo(B_MESSAGE_TYPE, index, &name, &type,
			&count) == B_OK; index++) {
		BMessage message;
		for (int32 messageIndex = 0; args.FindMessage(name, messageIndex,
				&message) == B_OK; messageIndex++) {
			AddCondition(create_condition(name, message));
		}
	}
}


ConditionContainer::ConditionContainer()
	:
	fConditions(10, true)
{
}


void
ConditionContainer::AddCondition(Condition* condition)
{
	if (condition != NULL)
		fConditions.AddItem(condition);
}


/*!	A single constant failing condition makes this constant, too, otherwise,
	a single non-constant condition makes this non-constant as well.
*/
bool
ConditionContainer::IsConstant(ConditionContext& context) const
{
	bool fixed = true;
	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		const Condition* condition = fConditions.ItemAt(index);
		if (condition->IsConstant(context)) {
			if (!condition->Test(context))
				return true;
		} else
			fixed = false;
	}
	return fixed;
}


void
ConditionContainer::ToString(BString& string) const
{
	string += "[";

	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		if (index != 0)
			string += ", ";
		string += fConditions.ItemAt(index)->ToString();
	}
	string += "]";
}


// #pragma mark - and


AndCondition::AndCondition(const BMessage& args)
	:
	ConditionContainer(args)
{
}


AndCondition::AndCondition()
{
}


bool
AndCondition::Test(ConditionContext& context) const
{
	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		Condition* condition = fConditions.ItemAt(index);
		if (!condition->Test(context))
			return false;
	}
	return true;
}


BString
AndCondition::ToString() const
{
	BString string = "and ";
	ConditionContainer::ToString(string);
	return string;
}


// #pragma mark - or


OrCondition::OrCondition(const BMessage& args)
	:
	ConditionContainer(args)
{
}


bool
OrCondition::Test(ConditionContext& context) const
{
	if (fConditions.IsEmpty())
		return true;

	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		Condition* condition = fConditions.ItemAt(index);
		if (condition->Test(context))
			return true;
	}
	return false;
}


/*!	If there is a single succeeding constant condition, this is constant, too.
	Otherwise, it is non-constant if there is a single non-constant condition.
*/
bool
OrCondition::IsConstant(ConditionContext& context) const
{
	bool fixed = true;
	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		const Condition* condition = fConditions.ItemAt(index);
		if (condition->IsConstant(context)) {
			if (condition->Test(context))
				return true;
		} else
			fixed = false;
	}
	return fixed;
}


BString
OrCondition::ToString() const
{
	BString string = "or ";
	ConditionContainer::ToString(string);
	return string;
}


// #pragma mark - or


NotCondition::NotCondition(const BMessage& args)
	:
	ConditionContainer(args)
{
}


NotCondition::NotCondition()
{
}


bool
NotCondition::Test(ConditionContext& context) const
{
	for (int32 index = 0; index < fConditions.CountItems(); index++) {
		Condition* condition = fConditions.ItemAt(index);
		if (condition->Test(context))
			return false;
	}
	return true;
}


BString
NotCondition::ToString() const
{
	BString string = "not ";
	ConditionContainer::ToString(string);
	return string;
}


// #pragma mark - safemode


bool
SafeModeCondition::Test(ConditionContext& context) const
{
	return context.IsSafeMode();
}


bool
SafeModeCondition::IsConstant(ConditionContext& context) const
{
	return true;
}


BString
SafeModeCondition::ToString() const
{
	return "safemode";
}


// #pragma mark - read_only


ReadOnlyCondition::ReadOnlyCondition(const BMessage& args)
	:
	fPath(args.GetString("args")),
	fIsReadOnly(false),
	fTestPerformed(false)
{
}


bool
ReadOnlyCondition::Test(ConditionContext& context) const
{
	if (fTestPerformed)
		return fIsReadOnly;

	if (fPath.IsEmpty() || fPath == "/boot")
		fIsReadOnly = context.BootVolumeIsReadOnly();
	else
		fIsReadOnly = Utility::IsReadOnlyVolume(fPath);

	fTestPerformed = true;

	return fIsReadOnly;
}


bool
ReadOnlyCondition::IsConstant(ConditionContext& context) const
{
	return true;
}


BString
ReadOnlyCondition::ToString() const
{
	BString string = "readonly ";
	string << fPath;
	return string;
}


// #pragma mark - file_exists


FileExistsCondition::FileExistsCondition(const BMessage& args)
{
	for (int32 index = 0;
			const char* path = args.GetString("args", index, NULL); index++) {
		fPaths.Add(path);
	}
}


bool
FileExistsCondition::Test(ConditionContext& context) const
{
	for (int32 index = 0; index < fPaths.CountStrings(); index++) {
		BEntry entry;
		if (entry.SetTo(fPaths.StringAt(index)) != B_OK
			|| !entry.Exists())
			return false;
	}
	return true;
}


BString
FileExistsCondition::ToString() const
{
	BString string = "file_exists [";
	for (int32 index = 0; index < fPaths.CountStrings(); index++) {
		if (index != 0)
			string << ", ";
		string << fPaths.StringAt(index);
	}
	string += "]";
	return string;
}


// #pragma mark -


/*static*/ Condition*
Conditions::FromMessage(const BMessage& message)
{
	return create_condition("and", message);
}


/*static*/ Condition*
Conditions::AddNotSafeMode(Condition* condition)
{
	AndCondition* and = dynamic_cast<AndCondition*>(condition);
	if (and == NULL)
		and = new AndCondition();
	if (and != condition && condition != NULL)
		and->AddCondition(condition);

	NotCondition* not = new NotCondition();
	not->AddCondition(new SafeModeCondition());

	and->AddCondition(not);
	return and;
}
