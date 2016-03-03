
json='{
   "active_patient": true,
   "last_visit": "2015/02/03",
	"referred_by": Null,

   "patient_info": {
        "first_name": "Fred",
        "last_name": "Flintsone",
        "dob": "1963/04/03",
        "sex": "M",
        "weight_kilo": 65.2,
        "drug_alergies": [ "asprin","darvaset" ]
    }

	"notes": {
		"over_weight": true,
		"smoker": false,
		"weight_trend": "gaining",
	}

    "Contact_info": {
        "name": "Wilma", "relation": "wife", "phone": "972.612.8918"
    }
}'

jwrapper_test "$json" "patient_info.dob" "patient_info.drug_alergies" 1
jwrapper_test "$json" "last_visit"
jwrapper_test "$json" "patient_info.weight_kilo"
jwrapper_test "$json" "notes.over_weight"
echo ""
echo "testing for value which is null -- should result in not found"
jwrapper_test "$json" "referred_by"


