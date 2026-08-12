import json
import sys
sys.path.append("../build")

import stackclone_module

with open("Config/config.json", "r") as file:
	data = json.load(file)
db_url = data["database"]["db_link"]
db_name = data["database"]["db_name"]
table_name = data["database"]["table_name"]

MySql = stackclone_module.db_MySql(db_url)

def search_post(Id):
	return MySql.get_post(Id, db_name, table_name)

def main():
	while (1):
		print("Python API Layer Test:", end="\n")

		print("1. Search Posts", end="\n")

		print("------------", end="\n")

		usr_input = input("Enter option: ")
		if usr_input.isalpha():
			print("Invalid input, please try again")
			print("\n")
			continue

		if int(usr_input) == 1:
			result = search_post(int(input("Enter Post Id: ")))
			print("\n")
			print(result)
			print("\n")
		else:
			print("Invalid input, please try again", end="\n")

if __name__ == "__main__":
	main()

print(result)
