CREATE TABLE IF NOT EXISTS Badges ( 
Id INT,
Name VARCHAR(40),
UserId INT,
Date DATETIME
);

CREATE TABLE IF NOT EXISTS Comments (
Id INT,
CreationDate DATETIME,
PostId INT,
Score INT,
Text VARCHAR(700),
UserId INT
);

CREATE TABLE IF NOT EXISTS VoteTypes (
Id INT,
Name VARCHAR(50)
);

CREATE TABLE IF NOT EXISTS Users (

Id INT,
AboutMe MEDIUMTEXT,
Age INT,
CreationDate DATETIME,
DisplayName VARCHAR(40),
DownVotes INT,
EmailHash VARCHAR(40),
LastAccessDate DATETIME,
Location VARCHAR(100),
Reputation INT,
UpVotes INT,
Views INT,
WebsiteUrl VARCHAR(200),
AccountId INT

);

CREATE TABLE IF NOT EXISTS PostTypes (
Id INT,
Type VARCHAR(50)
);

CREATE TABLE IF NOT EXISTS Posts (
Id INT,
AcceptedAnswerId INT,
AnswerCount INT,
Body MEDIUMTEXT,
ClosedDate DATETIME,
CommentCount INT,
CommunityOwnedDate DATETIME,
CreationDate DATETIME,
FavoriteCount INT,
LastActivityDate DATETIME,
LastEditDate DATETIME,
LastEditorDisplayName VARCHAR(40),
LastEditorUserId INT,
OwnerUserId INT,
ParentId INT,
PostTypeId INT,
Score INT,
Tags VARCHAR(150),
Title VARCHAR(250),
ViewCount INT
);

CREATE TABLE IF NOT EXISTS LinkTypes (
Id INT,
Type VARCHAR(50)
);

CREATE TABLE IF NOT EXISTS PostLinks (
Id INT,
CreationDate DATETIME,
PostId INT,
RelatedPostId INT,
LinkTypeId INT
);

CREATE TABLE IF NOT EXISTS Votes (
Id INT,
PostId INT,
UserId INT,
BountyAmount INT,
VoteTypeId INT,
CreationDate DATETIME
);